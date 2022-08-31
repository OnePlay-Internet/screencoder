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

        Config* config;
    };


    /**
     * @brief 
     * 
     * @param frame_nr 
     * @param session 
     * @param frame 
     * @param packets 
     * @return int 
     */
    util::Buffer* 
    encode(int frame_nr, 
           EncodeContext* session, 
           libav::Frame* frame) 
    {
        libav::CodecContext* libav_ctx = session->context;
        frame->pts = (int64_t)frame_nr;

        /* send the frame to the encoder */
        int ret = avcodec_send_frame(libav_ctx, frame);
        if(ret < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret));
            NEW_ERROR(error::Error::ENCODER_UNKNOWN);
        }


        {
            libav::Packet* packet = av_packet_alloc();
            ret = avcodec_receive_packet(libav_ctx, packet);
            if(ret == AVERROR(EAGAIN)) {
                NEW_ERROR(error::Error::ENCODER_NEED_MORE_FRAME);
            } else if(ret == AVERROR_EOF) {
                NEW_ERROR(error::Error::ENCODER_END_OF_STREAM);
            } else if(ret < 0) {
                NEW_ERROR(error::Error::ENCODER_UNKNOWN);
            }

            return BUFFER_INIT(packet,sizeof(libav::Packet),libav::packet_free_func);
        }
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
        util::Buffer* imgBuf = ctx->display->klass->alloc_img(ctx->display);

        platf::Image* img1 = (platf::Image*)BUFFER_REF(imgBuf,NULL);
        error::Error err = ctx->display->klass->dummy_img(ctx->display,img1);
        BUFFER_UNREF(imgBuf);

        if(FILTER_ERROR(err)) 
            RAISE_EVENT(ctx->shutdown_event);

        

        util::Buffer* ctxBuf = make_encode_context_buffer(ctx->encoder, ctx->config,
                                                ctx->display,
                                                ctx->sink);

        if(FILTER_ERROR(ctxBuf)) 
            RAISE_EVENT(ctx->shutdown_event);
        
        EncodeContext* session0 = (EncodeContext*)BUFFER_REF(ctxBuf,NULL);
        util::Buffer* frameBuf = make_avframe_buffer(session0);
        BUFFER_UNREF(ctxBuf);

        // run image capture in while loop, 
        while (!IS_INVOKED(ctx->shutdown_event))
        {
            platf::Capture ret;

            platf::Image* img2 = (platf::Image*)BUFFER_REF(imgBuf,NULL);
            ret = ctx->display->klass->capture(ctx->display, img2, FALSE);
            BUFFER_UNREF(imgBuf);

            if (ret == platf::Capture::timeout){
                LOG_ERROR("Capture timeout");
                continue;
            } else if (ret == platf::Capture::error) {
                LOG_ERROR("Could not encode video packet");
                break;
            }

            {
                // convert image
                platf::Image* img3 = (platf::Image*)BUFFER_REF(imgBuf,NULL);
                libav::Frame* frame0 = (libav::Frame*)BUFFER_REF(frameBuf,NULL);
                EncodeContext* session1 = (EncodeContext*)BUFFER_REF(ctxBuf,NULL);
                err = session1->device->klass->convert(session1->device,img3,frame0);
                BUFFER_UNREF(ctxBuf);
                BUFFER_UNREF(frameBuf);
                BUFFER_UNREF(imgBuf);

                if(FILTER_ERROR(err)) {
                    LOG_ERROR("Could not convert video packet");
                    RAISE_EVENT(ctx->shutdown_event);
                    continue;
                }

                // encode
                EncodeContext* session2     = (EncodeContext*)BUFFER_REF(ctxBuf,NULL);
                libav::Frame* frame1 = (libav::Frame*)BUFFER_REF(frameBuf,NULL);
                util::Buffer* avbuf = encode(ctx->frame_nr++, session2, frame1);
                BUFFER_UNREF(frameBuf);
                BUFFER_UNREF(ctxBuf);



                if(FILTER_ERROR(avbuf)) {
                    error::Error err = FILTER_ERROR(avbuf);
                    if (err == error::Error::ENCODER_END_OF_STREAM ||
                        err == error::Error::ENCODER_NEED_MORE_FRAME){
                        continue;
                    }
                    
                    LOG_ERROR("Could not encode video packet");
                    RAISE_EVENT(ctx->shutdown_event);
                    break;
                } 


                QUEUE_ARRAY_CLASS->push(ctx->packet_queue, avbuf);
                BUFFER_UNREF(avbuf);
            }
        }
        
        BUFFER_UNREF(ctxBuf);
        BUFFER_UNREF(imgBuf);
        BUFFER_UNREF(frameBuf);
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
             encoder::Config* config,
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
        ss_ctx.config = config;


        ss_ctx.thread = std::thread {captureThread, &ss_ctx };

        // Wait for join signal
        WAIT_EVENT(join_event);
    }




}