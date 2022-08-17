/**
 * @file encoder.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_thread.h>

#include <screencoder_util.h>

#include <screencoder_config.h>
#include <encoder_device.h>

#include <platform_common.h>
#include <display_base.h>
#include <generic_sink.h>


extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <encoder_session.h>
#include <windows_helper.h>

#include <thread>
#include <string.h>


using namespace std::literals;

namespace encoder {
    struct _EncodeThreadContext {
        int frame_nr;

        util::Broadcaster* shutdown_event;
        util::Broadcaster* join_event;
        util::QueueArray* packet_queue;

        std::thread thread;

        encoder::Encoder* encoder;
        platf::Display* display;
        sink::GenericSink* sink;
    };

    void
    free_av_packet(void* pkt)
    {
        av_free_packet((libav::Packet*)pkt);
    }


    /**
     * @brief 
     * 
     * @param frame_nr 
     * @param session 
     * @param frame 
     * @param packets 
     * @return int 
     */
    bool
    encode(int frame_nr, 
           util::Buffer* session_buf, 
           libav::Frame* frame, 
           util::QueueArray* packets) 
    {
        int ret;
        util::Buffer* pkt = NULL;
        Session* session      = (Session*)BUFFER_CLASS->ref(session_buf,NULL);
        libav::CodecContext* libav_ctx = session->encode->context;
        frame->pts = (int64_t)frame_nr;

        /* send the frame to the encoder */
        ret = avcodec_send_frame(libav_ctx, frame);
        if(ret < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret));
            BUFFER_CLASS->unref(session_buf);
            return FALSE;
        }


        libav::Packet* packet = av_packet_alloc();
        ret = avcodec_receive_packet(libav_ctx, packet);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            BUFFER_CLASS->unref(session_buf);
            return TRUE;
        } else if(ret < 0) {
            BUFFER_CLASS->unref(session_buf);
            return FALSE;
        }

        // we pass the reference of session to packet
        pkt = BUFFER_CLASS->init(packet,sizeof(libav::Packet),free_av_packet);
        QUEUE_ARRAY_CLASS->push(packets,pkt);
        BUFFER_CLASS->unref(session_buf);
        BUFFER_CLASS->unref(pkt);
        return TRUE;
    }





    /**
     * @brief 
     * 
     * @param img 
     * @param syncsession 
     * @return platf::Capture 
     */
    platf::Capture
    on_image_snapshoot (platf::Image** img,
                        util::Buffer* buffer,
                        EncodeThreadContext* thread_ctx)
    {
        Session* session = (Session*)BUFFER_CLASS->ref(buffer,NULL);
        platf::Device* device = session->encode->device;

        // get frame from device
        libav::Frame* frame = device->frame;

        // shutdown while loop whenever shutdown event happen
        if(IS_INVOKED(thread_ctx->shutdown_event)) {
            // Let waiting thread know it can delete shutdown_event
            RAISE_EVENT(thread_ctx->join_event);
            BUFFER_CLASS->unref(buffer);
            return platf::Capture::error;
        }

        // convert image
        if(device->klass->convert(device,*img)) {
            LOG_ERROR("Could not convert image");
            RAISE_EVENT(thread_ctx->shutdown_event);
            BUFFER_CLASS->unref(buffer);
            return platf::Capture::error;
        }

        // encode
        if(!encode(thread_ctx->frame_nr++, 
                buffer, 
                frame, 
                thread_ctx->packet_queue)) {
            LOG_ERROR("Could not encode video packet");
            RAISE_EVENT(thread_ctx->shutdown_event);
            BUFFER_CLASS->unref(buffer);
            return platf::Capture::error;
        }

        // reset keyframe attribute
        // frame->pict_type = AV_PICTURE_TYPE_NONE;
        // frame->key_frame = 0;

        BUFFER_CLASS->unref(buffer);
        return platf::Capture::ok;
    }





    /**
     * @brief 
     * 
     * @param ctx 
     */
    void 
    captureThread(EncodeThreadContext* ctx) 
    {
        // allocate display image and intialize with dummy data
        platf::Image* img = ctx->display->klass->alloc_img(ctx->display);
        if(!img || ctx->display->klass->dummy_img(ctx->display,img)) 
            return;

        util::Buffer* buf = make_session_buffer(img, 
                                                ctx->encoder,
                                                ctx->display,
                                                ctx->sink);
        if(!buf)
            return;

        // run image capture in while loop, 
        platf::Capture ret = ctx->display->klass->capture(ctx->display, 
                                                    img,
                                                    (platf::SnapshootCallback)on_image_snapshoot, 
                                                    buf, 
                                                    ctx,
                                                    FALSE);
        
        BUFFER_CLASS->unref(buf);
        
        RAISE_EVENT(ctx->shutdown_event);
        RAISE_EVENT(ctx->join_event);
    }

    /**
     * @brief 
     * 
     * @param shutdown_event 
     * @param packet_queue 
     * @param config 
     * @param data 
     */
    void 
    capture( platf::Display* capture,
             encoder::Encoder* encoder,
             sink::GenericSink* sink,
             util::Broadcaster* shutdown_event,
             util::QueueArray* packet_queue) 
    {
        util::Broadcaster* join_event = NEW_EVENT;

        // push new session context to concode queue
        EncodeThreadContext ss_ctx = {0};
        ss_ctx.shutdown_event = shutdown_event;
        ss_ctx.join_event = join_event;
        ss_ctx.packet_queue = packet_queue;
        ss_ctx.frame_nr = 1;

        ss_ctx.display = capture;
        ss_ctx.encoder = encoder;
        ss_ctx.sink = sink;


        ss_ctx.thread = std::thread {captureThread, &ss_ctx };

        // Wait for join signal
        WAIT_EVENT(join_event);
    }




}