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

#include <sunshine_util.h>

#include <encoder_d3d11_device.h>
#include <sunshine_config.h>
#include <encoder_device.h>

#include <platform_common.h>
#include <display_base.h>


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

        Config* config;

        encoder::Encoder* encoder;
        platf::Display* display;
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
    int 
    encode(int frame_nr, 
           util::Buffer* session_buf, 
           libav::Frame* frame, 
           util::QueueArray* packets) 
    {
        Session* session = (Session*)BUFFER_CLASS->ref(session_buf,NULL);
        EncodeContext* encode = &session->encode;


        libav::CodecContext* libav_ctx = encode->context;
        frame->pts = (int64_t)frame_nr;

        /* send the frame to the encoder */
        auto ret = avcodec_send_frame(libav_ctx, frame);
        if(ret < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret));
            return FALSE;
        }


        session->packet = av_packet_alloc();
        ret = avcodec_receive_packet(libav_ctx, session->packet);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
            return 0;
        else if(ret < 0) 
            return ret;

        // we pass the reference of session to packet

        QUEUE_ARRAY_CLASS->push(packets,session_buf);
        BUFFER_CLASS->unref(session_buf);
        return 0;
    }





    /**
     * @brief 
     * 
     * @param img 
     * @param syncsession 
     * @return platf::Capture 
     */
    platf::Capture
    on_image_snapshoot (platf::Image* img,
                        util::Buffer* buffer,
                        EncodeThreadContext* thread_ctx)
    {
        Session* session = (Session*)BUFFER_CLASS->ref(buffer,NULL);
        platf::Device* device = session->encode.device;

        // get frame from device
        libav::Frame* frame = device->frame;

        // shutdown while loop whenever shutdown event happen
        if(IS_INVOKED(thread_ctx->shutdown_event)) {
            // Let waiting thread know it can delete shutdown_event
            RAISE_EVENT(thread_ctx->join_event);
            return platf::Capture::error;
        }

        // convert image
        if(device->klass->convert(device,img)) {
            LOG_ERROR("Could not convert image");
            RAISE_EVENT(thread_ctx->shutdown_event);
            return platf::Capture::error;
        }

        // encode
        if(encode(thread_ctx->frame_nr++, 
                  buffer, 
                  frame, 
                  thread_ctx->packet_queue)) 
        {
            LOG_ERROR("Could not encode video packet");
            RAISE_EVENT(thread_ctx->shutdown_event);
            return platf::Capture::error;
        }

        // reset keyframe attribute
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        frame->key_frame = 0;

        BUFFER_CLASS->unref(buffer);
        return platf::Capture::ok;
    }




    // TODO add encoder and display to context
    platf::Capture
    encode_run_sync(EncodeThreadContext* ctx) 
    {
        // allocate display image and intialize with dummy data
        platf::Image* img = ctx->display->klass->alloc_img(ctx->display);
        if(!img || ctx->display->klass->dummy_img(ctx->display,img)) 
            return platf::Capture::error;

        util::Buffer* buf = make_synced_session(img, 
                                                ctx->encoder,
                                                ctx->display,
                                                ctx->config);
        if(!buf)
            return platf::Capture::error;

        // cursor
        // run image capture in while loop, 
        return (platf::Capture)ctx->display->klass->capture(ctx->display, 
                                                    img,
                                                    (platf::SnapshootCallback)on_image_snapshoot, 
                                                    buf, TRUE);
    }

    /**
     * @brief 
     * 
     * @param ctx 
     */
    void 
    captureThread(EncodeThreadContext* ctx) 
    {
        // start capture thread sync thread and create a reference to its context
        platf::Display* disp;
        Encoder* encoder = NVENC;

        // display selection
        {
            char* chosen_display  = NULL;
            char** display_names  = platf::display_names(helper::map_dev_type(encoder->dev_type));

            if(!display_names) 
                chosen_display = ENCODER_CONFIG->output_name;

            int count = 0;
            while (*(display_names+count))
            {
                if(*(display_names+count) == ENCODER_CONFIG->output_name) {
                    chosen_display = *(display_names+count);
                    break;
                }
                count++;
            }

            // reset display every 200ms until display is ready
            disp = platf::tryget_display(encoder->dev_type, 
                                         chosen_display, 
                                         ENCODER_CONFIG->framerate);
            if(!disp) {
                LOG_ERROR("unable to create display");
                goto done;
            }
        }

        ctx->display = disp;
        ctx->encoder = encoder;


        // run encoder in infinite loop
        while(TRUE) {
            platf::Capture result = encode_run_sync(ctx);
            switch (result)
            {
            case platf::Capture::error:
                goto done;
            default:
                continue;
            }
        }
        done:
        RAISE_EVENT(ctx->shutdown_event);
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
    capture( util::Broadcaster* shutdown_event,
             util::QueueArray* packet_queue,
             Config* config) 
    {
        util::Broadcaster* join_event = NEW_EVENT;

        // push new session context to concode queue
        EncodeThreadContext ss_ctx = {0};
        ss_ctx.shutdown_event = shutdown_event;
        ss_ctx.join_event = join_event;
        ss_ctx.packet_queue = packet_queue;

        ss_ctx.config = config;
        ss_ctx.frame_nr = 1;
        ss_ctx.thread = std::thread {captureThread, &ss_ctx };

        // Wait for join signal
        WAIT_EVENT(join_event);
    }




}