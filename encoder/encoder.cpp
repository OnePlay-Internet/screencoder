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
#include <thread>


#include <sunshine_queue.h>
#include <sunshine_log.h>
#include <sunshine_object.h>
#include <sunshine_bitstream.h>
#include <sunshine_event.h>

#include <encoder_datatype.h>
#include <encoder_packet.h>
#include <encoder_d3d11_device.h>
#include <common.h>
#include <display.h>



namespace encoder {
    const char* hevc_nalu = "\000\000\000\001(";
    const char* h264_nalu = "\000\000\000\001e";

    void do_nothing(pointer data){};

    typedef enum _Status {
        ok,
        reinit,
        timeout,
        error
    }Status;

    /**
     * @brief 
     * 
     */
    typedef struct _SyncSessionContext{
        event::Broadcaster* shutdown_event;
        event::Broadcaster* idr_event;
        event::Broadcaster* join_event;

        util::QueueArray* packet_queue;

        int frame_nr;
        Config config;
        pointer channel_data;
    }SyncSessionContext;

    typedef struct _Session {
        libav::CodecContext context;
        platf::HWDevice* device;

        Replace* replacements;

        bitstream::NAL sps;
        bitstream::NAL vps;

        int inject;
    }Session;

    typedef struct _CaptureThreadSyncContext {
        object::Object base_object;

        /**
         * @brief 
         * SyncedSessionContext queue
         */
        util::QueueArray* sync_session_queue;
    }CaptureThreadSyncContext;


    Status
    encode_run_sync(SyncSessionContext* synced_session_ctxs,
                    util::QueueArray* encode_session_ctx_queue) 
    {
        const Encoder* encoder = NVENC;
        auto display_names  = platf::display_names(map_dev_type(encoder.dev_type));
        int display_p       = 0;

        if(display_names.empty()) {
            display_names.emplace_back(config::video.output_name);
        }

        for(int x = 0; x < display_names.size(); ++x) {
            if(display_names[x] == config::video.output_name) {
            display_p = x;

            break;
            }
        }

        platf::Display* disp;
        auto img = disp->alloc_img();


        // auto switch_display_event = mail::man->event<int>(mail::switch_display);

        // pop context from context queue and move them to synced session queue
        // if(synced_session_ctxs.empty()) {
        //     auto ctx = encode_session_ctx_queue.pop();
        //     if(!ctx) {
        //     return encode_e::ok;
        //     }

        //     synced_session_ctxs.emplace_back(std::make_unique<sync_session_ctx_t>(std::move(*ctx)));
        // }

        // read framerate from synced session 
        // int framerate = synced_session_ctxs.front()->config.framerate;

        // // reset display every 200ms until display is ready
        // while(encode_session_ctx_queue.running()) {
        //     reset_display(disp, encoder.dev_type, display_names[display_p], framerate);
        //     if(disp) {
        //     break;
        //     }

        //     std::this_thread::sleep_for(200ms);
        // }

        // if(!disp) {
        //     return encode_e::error;
        // }

        // // allocate display image and intialize with dummy data
        // auto img = disp->alloc_img();
        // if(!img || disp->dummy_img(img.get())) {
        //     return encode_e::error;
        // }

        // // create one session for easch synced session context
        // std::vector<sync_session_t> synced_sessions;
        // for(auto &ctx : synced_session_ctxs) {
        //     auto synced_session = make_synced_session(disp.get(), encoder, *img, *ctx);
        //     if(!synced_session) {
        //     return encode_e::error;
        //     }

        //     synced_sessions.emplace_back(std::move(*synced_session));
        // }


        // return status
        // auto ec = Status::ok;
        // while(encode_session_ctx_queue.running()) {
        //     auto snapshot_cb = [&](std::shared_ptr<platf::Image> &img) -> std::shared_ptr<platf::Image> {
        //     while(encode_session_ctx_queue.peek()) {
        //         auto encode_session_ctx = encode_session_ctx_queue.pop();
        //         if(!encode_session_ctx) {
        //         return nullptr;
        //         }

        //         synced_session_ctxs.emplace_back(std::make_unique<sync_session_ctx_t>(std::move(*encode_session_ctx)));

        //         auto encode_session = make_synced_session(disp.get(), encoder, *img, *synced_session_ctxs.back());
        //         if(!encode_session) {
        //         ec = Status::error;
        //         return nullptr;
        //         }

        //         synced_sessions.emplace_back(std::move(*encode_session));
        //     }

        //     KITTY_WHILE_LOOP(auto pos = std::begin(synced_sessions), pos != std::end(synced_sessions), {

        //         // get frame from device
        //         auto frame = pos->session.device->frame;

        //         // get avcodec context from synced session
        //         auto ctx   = pos->ctx;

        //         // shutdown while loop whenever shutdown event happen
        //         if(ctx->shutdown_event->peek()) {
        //         // Let waiting thread know it can delete shutdown_event
        //         ctx->join_event->raise(true);

        //         pos = synced_sessions.erase(pos);
        //         synced_session_ctxs.erase(std::find_if(std::begin(synced_session_ctxs), std::end(synced_session_ctxs), [&ctx_p = ctx](auto &ctx) {
        //             return ctx.get() == ctx_p;
        //         }));

        //         if(synced_sessions.empty()) {
        //             return nullptr;
        //         }

        //         continue;
        //         }

        //         // peek idr event (keyframe)
        //         if(ctx->idr_events->peek()) {
        //         frame->pict_type = AV_PICTURE_TYPE_I;
        //         frame->key_frame = 1;

        //         ctx->idr_events->pop();
        //         }

        //         //setup hardware device context with base image profile
        //         if(pos->session.device->convert(*img)) {
        //         BOOST_LOG(error) << "Could not convert image"sv;
        //         ctx->shutdown_event->raise(true);

        //         continue;
        //         }

        //         // encode
        //         if(encode(ctx->frame_nr++, pos->session, frame, ctx->packets, ctx->channel_data)) {
        //             LOG_ERROR("Could not encode video packet");
        //             ctx->shutdown_event->raise(true);
        //             continue;

        //         }

        //         // reset keyframe attribute
        //         frame->pict_type = AV_PICTURE_TYPE_NONE;
        //         frame->key_frame = 0;

        //         ++pos;
        //     })

        //     // switch display
        //     if(switch_display_event->peek()) {
        //         ec = Status::reinit;

        //         display_p = std::clamp(*switch_display_event->pop(), 0, (int)display_names.size() - 1);
        //         return nullptr;
        //     }

        //     return img;
        //     };

        //     // run image capture in while loop
        //     auto status = disp->capture(std::move(snapshot_cb), img, &display_cursor);

        //     // return for timeout status
        //     switch(status) {
        //     case Status::reinit:
        //     case Status::error:
        //     case Status::ok:
        //     case Status::timeout:
        //     return ec != Status::ok ? ec : status;
        //     }
        // }
        return Status::ok;
    }

    void captureThreadSync(CaptureThreadSyncContext* ctx) {
        // start capture thread sync thread and create a reference to its context
        SyncSessionContext* synced_session_ctxs;


        while(encode_run_sync(synced_session_ctxs, ctx->sync_session_queue) == Status::reinit) {}


        QUEUE_ARRAY_CLASS->stop(ctx->sync_session_queue);
        
        int i = 0;
        while ((synced_session_ctxs + i) != NULL)
        {
            SyncSessionContext ss_ctx = *(synced_session_ctxs + i);
            RAISE_EVENT(ss_ctx->shutdown_event);
            RAISE_EVENT(ss_ctx->join_event);
            i++;
        }

        while (QUEUE_ARRAY_CLASS->peek(ctx->sync_session_queue))
        {
            SyncSessionContext* ss_ctx = (SyncSessionContext*) obj->data;
            RAISE_EVENT(ss_ctx->shutdown_event);
            RAISE_EVENT(ss_ctx->join_event);
            i++;
        }
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
    capture( event::Broadcaster* shutdown_event,
             util::QueueArray* packet_queue,
             Config config,
             pointer data) 
    {
        event::Broadcaster* join_event = NEW_EVENT;
        event::Broadcaster* idr_event = NEW_EVENT;
        RAISE_EVENT(idr_event);

        // start capture thread sync and let it manages its own context
        CaptureThreadSyncContext ctx = {0};
        captureThreadSync(&ctx);

        // push new session context to concode queue
        SyncSessionContext ss_ctx = {
            shutdown_event,
            idr_event,
            join_event,
            
            packet_queue,
            1,

            &config,
            data
        };

        object::Object* obj = OBJECT_CLASS->init(&ss_ctx,DO_NOTHING);
        QUEUE_ARRAY_CLASS->push(ctx.sync_session_queue,obj);

        // Wait for join signal
        while(!WAIT_EVENT(join_event)){ }
    }

    Color 
    make_color_matrix(float Cr, float Cb, 
                      float U_max, float V_max, 
                      float add_Y, float add_UV, 
                      const float2 &range_Y, const float2 &range_UV) 
    {
        float Cg = 1.0f - Cr - Cb;

        float Cr_i = 1.0f - Cr;
        float Cb_i = 1.0f - Cb;

        float shift_y  = range_Y[0] / 256.0f;
        float shift_uv = range_UV[0] / 256.0f;

        float scale_y  = (range_Y[1] - range_Y[0]) / 256.0f;
        float scale_uv = (range_UV[1] - range_UV[0]) / 256.0f;
        return {
            { Cr, Cg, Cb, add_Y },
            { -(Cr * U_max / Cb_i), -(Cg * U_max / Cb_i), U_max, add_UV },
            { V_max, -(Cg * V_max / Cr_i), -(Cb * V_max / Cr_i), add_UV },
            { scale_y, shift_y },
            { scale_uv, shift_uv },
        };
    }

    Color*
    get_color(){
        static bool init = false;
        static Color colors[4];
        if(init)
            return &colors;

        init = true; 
        colors = 
        {
            make_color_matrix(0.299f, 0.114f, 0.436f, 0.615f, 0.0625, 0.5f, { 16.0f, 235.0f }, { 16.0f, 240.0f }),   // BT601 MPEG
            make_color_matrix(0.299f, 0.114f, 0.5f, 0.5f, 0.0f, 0.5f, { 0.0f, 255.0f }, { 0.0f, 255.0f }),           // BT601 JPEG
            make_color_matrix(0.2126f, 0.0722f, 0.436f, 0.615f, 0.0625, 0.5f, { 16.0f, 235.0f }, { 16.0f, 240.0f }), // BT701 MPEG
            make_color_matrix(0.2126f, 0.0722f, 0.5f, 0.5f, 0.0f, 0.5f, { 0.0f, 255.0f }, { 0.0f, 255.0f }),         // BT701 JPEG
        };
        return &colors;
    }
}