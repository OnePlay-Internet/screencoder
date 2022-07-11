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
#include <encoder.h>

#include <sunshine_util.h>

#include <sunshine_bitstream.h>
#include <encoder_packet.h>
#include <encoder_d3d11_device.h>
#include <sunshine_config.h>

#include <common.h>
#include <display.h>
extern "C" {
#include <libswscale/swscale.h>
}

#include <thread>
#include <string.h>

using namespace std::literals;

namespace encoder {

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
        util::Broadcaster* shutdown_event;
        util::Broadcaster* idr_event;
        util::Broadcaster* join_event;

        util::QueueArray* packet_queue;

        int frame_nr;
        Config* config;
        Encoder* encoder;
        platf::Display* display;
        pointer channel_data;
    }SyncSessionContext;

    typedef struct _SyncSession{
        SyncSessionContext* ctx;
        Session session;
    }SyncSession;


    typedef struct _CaptureThreadSyncContext {
        /**
         * @brief 
         * SyncedSessionContext queue
         */
        util::QueueArray* sync_session_ctx_queue;
    }CaptureThreadSyncContext;





    /**
     * @brief 
     * 
     * @param ctx 
     * @param hwdevice_ctx 
     * @param format 
     * @return int 
     */
    int 
    hwframe_ctx(libav::CodecContext* ctx, 
                libav::BufferRef* hwdevice, 
                libav::PixelFormat format) 
    {
        libav::BufferRef* frame_ref = av_hwframe_ctx_alloc(hwdevice);

        AVHWFramesContext* frame_ctx = (AVHWFramesContext *)frame_ref->data;
        frame_ctx->format            = ctx->pix_fmt;
        frame_ctx->sw_format         = format;
        frame_ctx->height            = ctx->height;
        frame_ctx->width             = ctx->width;
        frame_ctx->initial_pool_size = 0;

        int err = av_hwframe_ctx_init(frame_ref);
        if( err < 0) 
            return err;
        
        ctx->hw_frames_ctx = av_buffer_ref(frame_ref);
        return 0;
    }

    /**
     * @brief 
     * 
     * @param frame_nr 
     * @param session 
     * @param frame 
     * @param packets 
     * @param channel_data 
     * @return int 
     */
    int 
    encode(int frame_nr, 
           Session* session, 
           libav::Frame* frame, 
           util::QueueArray* packets, 
           void *channel_data) 
    {
        frame->pts = (int64_t)frame_nr;

        libav::CodecContext* libav_ctx = (libav::CodecContext*)session->context;

        bitstream::NAL sps = session->sps;
        bitstream::NAL vps = session->vps;

        /* send the frame to the encoder */
        auto ret = avcodec_send_frame(libav_ctx, frame);
        if(ret < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret));
            return -1;
        }

        while(ret >= 0) {
            Packet* packet    = packet_class_init()->init();
            libav::Packet* av_packet = packet->packet;

            ret = avcodec_receive_packet(libav_ctx, av_packet);

            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
                return 0;
            else if(ret < 0) 
                return ret;

            if(session->inject) {
                if(session->inject == 1) {
                    bitstream::H264 h264 = bitstream::make_sps_h264(libav_ctx, av_packet);
                    sps = h264.sps;
                } else {
                    bitstream::HEVC hevc = bitstream::make_sps_hevc(libav_ctx, av_packet);

                    sps = hevc.sps;
                    vps = hevc.vps;

                    OBJECT_MALLOC(obj,sizeof(Replace),ptr);
                    ((Replace*)ptr)->old = vps.old;
                    ((Replace*)ptr)->_new = vps._new;
                    LIST_OBJECT_CLASS->emplace_back(session->replacement_array,obj);
                }

                session->inject = 0;
                OBJECT_MALLOC(obj,sizeof(Replace),ptr);
                ((Replace*)ptr)->old = sps.old;
                ((Replace*)ptr)->_new = sps._new;
                LIST_OBJECT_CLASS->emplace_back(session->replacement_array,obj);
            }

            packet->replacement_array = session->replacement_array;
            packet->user_data = channel_data;

            util::Object* obj = OBJECT_CLASS->init(packet,sizeof(Packet),packet_class_init()->finalize);
            QUEUE_ARRAY_CLASS->push(packets,obj);
        }

        return 0;
    }

    void
    handle_options(AVDictionary* options, 
                   KeyValue* keyvalue)
    {
        KeyValue* option = keyvalue;
        while(option) {
            if(option->type == Type::STRING)
                av_dict_set(&options,option->key,option->string_value,0);

            if(option->type == Type::INT)
                av_dict_set_int(&options,option->key,option->int_value,0);

            option++;
        }
    }

    bool
    make_session(Session* session,   
                 Encoder* encoder, 
                 Config* config, 
                 int width, int height, 
                 platf::HWDevice* hwdevice) 
    {
        bool hardware = encoder->dev_type != AV_HWDEVICE_TYPE_NONE;

        CodecConfig* video_format = (config->videoFormat == 0) ? &encoder->h264 : &encoder->hevc;
        if(!video_format->capabilities[FrameFlags::PASSED]) {
            // BOOST_LOG(error) << encoder->name << ": "sv << video_format->name << " mode not supported"sv;
            return FALSE;
        }

        if(config->dynamicRange && !video_format->capabilities[FrameFlags::DYNAMIC_RANGE]) {
            // BOOST_LOG(error) << video_format->name << ": dynamic range not supported"sv;
            return FALSE;
        }

        AVCodec* codec = avcodec_find_encoder_by_name(video_format->name);
        if(!codec) {
            // BOOST_LOG(error) << "Couldn't open ["sv << video_format->name << ']';
            return FALSE;
        }

        libav::CodecContext* ctx = avcodec_alloc_context3(codec);
        ctx->width     = config->width;
        ctx->height    = config->height;
        ctx->time_base = AVRational { 1, config->framerate };
        ctx->framerate = AVRational { config->framerate, 1 };

        if(config->videoFormat == 0) {
            ctx->profile = encoder->profile.h264_high;
        }
        else if(config->dynamicRange == 0) {
            ctx->profile = encoder->profile.hevc_main;
        }
        else {
            ctx->profile = encoder->profile.hevc_main_10;
        }

        // B-frames delay decoder output, so never use them
        ctx->max_b_frames = 0;

        // Use an infinite GOP length since I-frames are generated on demand
        ctx->gop_size = encoder->flags & LIMITED_GOP_SIZE ? INT16_MAX : INT_MAX;
        ctx->keyint_min = INT_MAX;

        if(config->numRefFrames == 0) {
            ctx->refs = video_format->capabilities[FrameFlags::REF_FRAMES_AUTOSELECT] ? 0 : 16;
        }
        else {
            // Some client decoders have limits on the number of reference frames
            ctx->refs = video_format->capabilities[FrameFlags::REF_FRAMES_RESTRICT] ? config->numRefFrames : 0;
        }

        ctx->flags |= (AV_CODEC_FLAG_CLOSED_GOP | AV_CODEC_FLAG_LOW_DELAY);
        ctx->flags2 |= AV_CODEC_FLAG2_FAST;

        ctx->color_range = (config->encoderCscMode & 0x1) ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

        int sws_color_space;
        switch(config->encoderCscMode >> 1) {
        case 0:
        default:
            // Rec. 601
            // BOOST_LOG(info) << "Color coding [Rec. 601]"sv;
            ctx->color_primaries = AVCOL_PRI_SMPTE170M;
            ctx->color_trc       = AVCOL_TRC_SMPTE170M;
            ctx->colorspace      = AVCOL_SPC_SMPTE170M;
            sws_color_space      = SWS_CS_SMPTE170M;
            break;

        case 1:
            // Rec. 709
            // BOOST_LOG(info) << "Color coding [Rec. 709]"sv;
            ctx->color_primaries = AVCOL_PRI_BT709;
            ctx->color_trc       = AVCOL_TRC_BT709;
            ctx->colorspace      = AVCOL_SPC_BT709;
            sws_color_space      = SWS_CS_ITU709;
            break;

        case 2:
            // Rec. 2020
            // BOOST_LOG(info) << "Color coding [Rec. 2020]"sv;
            ctx->color_primaries = AVCOL_PRI_BT2020;
            ctx->color_trc       = AVCOL_TRC_BT2020_10;
            ctx->colorspace      = AVCOL_SPC_BT2020_NCL;
            sws_color_space      = SWS_CS_BT2020;
            break;
        }
        // BOOST_LOG(info) << "Color range: ["sv << ((config->encoderCscMode & 0x1) ? "JPEG"sv : "MPEG"sv) << ']';

        libav::PixelFormat sw_fmt;
        if(config->dynamicRange == 0) {
            sw_fmt = encoder->static_pix_fmt;
        }
        else {
            sw_fmt = encoder->dynamic_pix_fmt;
        }

        // Used by cbs::make_sps_hevc
        ctx->sw_pix_fmt = sw_fmt;

        libav::BufferRef* hwdevice_ctx;
        if(hardware) {
            ctx->pix_fmt = encoder->dev_pix_fmt;

            libav::BufferRef* buf_or_error = encoder->make_hw_ctx_func(hwdevice);

            // check datatype
            // if(buf_or_error.has_right()) {
            //     return FALSE;
            // }

            hwdevice_ctx = buf_or_error;
            if(hwframe_ctx(ctx, hwdevice_ctx, sw_fmt)) 
                return FALSE;
            
            ctx->slices = config->slicesPerFrame;
        } else /* software */ {
            ctx->pix_fmt = sw_fmt;

            // Clients will request for the fewest slices per frame to get the
            // most efficient encode, but we may want to provide more slices than
            // requested to ensure we have enough parallelism for good performance.
            ctx->slices = MAX(config->slicesPerFrame, ENCODER_CONFIG->min_threads);
        }

        if(!video_format->capabilities[FrameFlags::SLICE]) {
            ctx->slices = 1;
        }

        ctx->thread_type  = FF_THREAD_SLICE;
        ctx->thread_count = ctx->slices;

        /**
         * @brief 
         * map from config to option here
         */
        AVDictionary *options = NULL;
        handle_options(options,video_format->options);
        if(video_format->capabilities[FrameFlags::CBR]) {
            auto bitrate        = config->bitrate * (hardware ? 1000 : 800); // software bitrate overshoots by ~20%
            ctx->rc_max_rate    = bitrate;
            ctx->rc_buffer_size = bitrate / 10;
            ctx->bit_rate       = bitrate;
            ctx->rc_min_rate    = bitrate;
        }
        else if(video_format->has_qp) {
            handle_options(options,video_format->qp);
        }
        else {
            LOG_ERROR("Couldn't set video quality");
            return FALSE;
        }

        if(auto status = avcodec_open2(ctx, codec, &options)) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            // BOOST_LOG(error)
            // << "Could not open codec ["sv
            // << video_format->name << "]: "sv
            // << av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, status);
            return FALSE;
        }

        libav::Frame* frame = av_frame_alloc();
        frame->format = ctx->pix_fmt;
        frame->width  = ctx->width;
        frame->height = ctx->height;


        if(hardware) 
            frame->hw_frames_ctx = av_buffer_ref(ctx->hw_frames_ctx);
        

        platf::HWDevice* device;
        if(!hwdevice->data) {
            // TODO software encoder
            // auto device_tmp = std::make_unique<swdevice_t>();
            // if(device_tmp->init(width, height, frame.get(), sw_fmt)) 
            // {
            //     return FALSE;
            // }

            // device = std::move(device_tmp);
        }
        else 
        {
            device = hwdevice;
        }

        if(device->klass->set_frame(device,frame)) 
            return FALSE;
        

        device->klass->set_colorspace(device,sws_color_space, ctx->color_range);

        session->context = ctx;
        session->device = device;
        // 0 ==> don't inject, 1 ==> inject for h264, 2 ==> inject for hevc
        session->inject = (1 - (int)video_format->capabilities[FrameFlags::VUI_PARAMETERS]) * (1 + config->videoFormat);

        // TODO
        if(!video_format->capabilities[FrameFlags::NALU_PREFIX_5b]) 
        {
            char* hevc_nalu = "\000\000\000\001(";
            char* h264_nalu = "\000\000\000\001e";
            char* nalu_prefix = config->videoFormat ? hevc_nalu : h264_nalu;

            OBJECT_MALLOC(obj,sizeof(Replace),temp);
            OBJECT_DUPLICATE(old_obj,sizeof(char),nalu_prefix,temp1);
            OBJECT_DUPLICATE(new_obj,strlen(nalu_prefix),nalu_prefix,temp2);


            ((Replace*)temp)->old = old_obj;
            ((Replace*)temp)->_new = new_obj;

            LIST_OBJECT_CLASS->emplace_back(session->replacement_array,obj);
        }

        return TRUE;
    }


    /**
     * @brief 
     * 
     * @param disp 
     * @param encoder 
     * @param img 
     * @param ctx 
     * @return SyncSession* 
     */
    SyncSession* 
    make_synced_session(platf::Image *img, 
                        SyncSessionContext *ctx) 
    {
        SyncSession* encode_session = (SyncSession*)malloc(sizeof(SyncSession));
        memset(encode_session,0,sizeof(SyncSession));

        platf::PixelFormat pix_fmt = ctx->config->dynamicRange == 0 ? 
                                        map_pix_fmt(ctx->encoder->static_pix_fmt) : 
                                        map_pix_fmt(ctx->encoder->dynamic_pix_fmt);

        platf::HWDevice* hwdevice = ctx->display->klass->make_hwdevice(ctx->display,pix_fmt);

        if(!hwdevice) 
            return FALSE;

        if(!make_session(&encode_session->session, ctx->encoder,ctx->config, img->width, img->height, hwdevice))
            return FALSE;

        encode_session->ctx = ctx;
        return encode_session;
    }

    void
    free_synced_session(void* data)
    {
        free(data);
    }


    /**
     * @brief 
     * 
     * @param img 
     * @param synced_session_ctxs 
     * @param encode_session_ctx_queue 
     */
    platf::Capture
    on_image_snapshoot (platf::Image* img,
                        util::ListObject* synced_sessions,
                        util::ListObject* synced_session_ctxs,
                        util::QueueArray* encode_session_ctx_queue)
    {
        while(QUEUE_ARRAY_CLASS->peek(encode_session_ctx_queue)) {
            util::Object* obj = QUEUE_ARRAY_CLASS->pop(encode_session_ctx_queue);
            SyncSessionContext* encode_session_ctx = (SyncSessionContext*)OBJECT_CLASS->ref(obj);

            if(!encode_session_ctx) 
                return platf::Capture::error;

            SyncSession* encode_session = make_synced_session(img, encode_session_ctx);

            if(!encode_session) 
                return platf::Capture::error;
            
            util::Object* obj2 = OBJECT_CLASS->init(encode_session,sizeof(SyncSession),free_synced_session);

            LIST_OBJECT_CLASS->emplace_back(synced_sessions,obj2);
            LIST_OBJECT_CLASS->emplace_back(synced_session_ctxs,obj);
            OBJECT_CLASS->unref(obj);
        }


        while (LIST_OBJECT_CLASS->has_data(synced_sessions,0))
        {
            util::Object* obj = LIST_OBJECT_CLASS->get_data(synced_sessions,0);
            SyncSession* pos = (SyncSession*) OBJECT_CLASS->ref(obj);

            // get frame from device
            libav::Frame* frame = pos->session.device->frame;

            // get avcodec context from synced session
            SyncSessionContext* ctx   = pos->ctx;

            // shutdown while loop whenever shutdown event happen
            if(IS_INVOKED(ctx->shutdown_event)) {
                // Let waiting thread know it can delete shutdown_event
                RAISE_EVENT(ctx->join_event);
                LIST_OBJECT_CLASS->finalize(synced_sessions);
                LIST_OBJECT_CLASS->finalize(synced_session_ctxs);
                return platf::Capture::error;
            }

            // peek idr event (keyframe)
            if(IS_INVOKED(ctx->idr_event)) {
                frame->pict_type = AV_PICTURE_TYPE_I;
                frame->key_frame = 1;
            }

            // convert image
            if(pos->session.device->klass->convert(pos->session.device,img)) {
                LOG_ERROR("Could not convert image");
                RAISE_EVENT(ctx->shutdown_event);
                continue;
            }

            // encode
            if(encode(ctx->frame_nr++, 
                      &pos->session, 
                      frame, 
                      ctx->packet_queue, 
                      ctx->channel_data)) 
            {
                LOG_ERROR("Could not encode video packet");
                RAISE_EVENT(ctx->shutdown_event);
                continue;
            }

            // reset keyframe attribute
            frame->pict_type = AV_PICTURE_TYPE_NONE;
            frame->key_frame = 0;


            OBJECT_CLASS->unref(obj);
        };
        return platf::Capture::ok;
    }


    platf::MemoryType
    map_dev_type(AVHWDeviceType type) {
        switch(type) {
        case AV_HWDEVICE_TYPE_D3D11VA:
            return platf::MemoryType::dxgi;
        case AV_HWDEVICE_TYPE_VAAPI:
            return platf::MemoryType::vaapi;
        case AV_HWDEVICE_TYPE_CUDA:
            return platf::MemoryType::cuda;
        case AV_HWDEVICE_TYPE_NONE:
            return platf::MemoryType::system;
        default:
            return platf::MemoryType::unknown;
        }
        return platf::MemoryType::unknown;
    } 

    platf::Display*
    reset_display(AVHWDeviceType type, 
                  char* display_name, 
                  int framerate) 
    {
        platf::Display* disp = NULL;
        // We try this twice, in case we still get an error on reinitialization
        for(int x = 0; x < 2; ++x) {
            disp = platf::display(map_dev_type(type), display_name, framerate);
            if (disp)
                break;

            std::this_thread::sleep_for(200ms);
        }
        return disp;
    }

    // TODO add encoder and display to context
    Status
    encode_run_sync(util::ListObject* synced_session_ctxs,
                    util::QueueArray* encode_session_ctx_queue) 
    {
        util::ListObject* synced_sessions = LIST_OBJECT_CLASS->init();
        SyncSessionContext* ctx = NULL;

        // pop context from context queue and move them to synced session queue
        {
            if(!LIST_OBJECT_CLASS->length(synced_session_ctxs)) 
            {
                util::Object* obj = QUEUE_ARRAY_CLASS->pop(encode_session_ctx_queue);
                ctx = (SyncSessionContext*) (OBJECT_CLASS->ref(obj));

                if(!ctx) 
                    return Status::ok;

                OBJECT_CLASS->unref(obj);
                LIST_OBJECT_CLASS->emplace_back(synced_session_ctxs,obj);
            }
        }

        // allocate display image and intialize with dummy data
        platf::Image* img = ctx->display->klass->alloc_img(ctx->display);
        if(!img || ctx->display->klass->dummy_img(ctx->display,img)) {
            return Status::error;
        }

        // create one session for easch synced session context
        while(LIST_OBJECT_CLASS->has_data(synced_session_ctxs,0)) 
        {
            util::Object* obj = LIST_OBJECT_CLASS->get_data(synced_session_ctxs,0);

            SyncSessionContext* ctx = (SyncSessionContext*)OBJECT_CLASS->ref(obj);
            SyncSession *synced_session = make_synced_session(img, ctx);

            if(!synced_session) 
                return Status::error;

            {
                util::Object* ss_obj = OBJECT_CLASS->init(synced_session,sizeof(SyncSession),free_synced_session);
                LIST_OBJECT_CLASS->emplace_back(synced_sessions,ss_obj);
                OBJECT_CLASS->unref(ss_obj);
            }
            OBJECT_CLASS->unref(obj);
        }

        // return status
        Status ec = Status::ok;
        while(TRUE) {
            // cursor
            // run image capture in while loop, 
            Status status = (Status)ctx->display->klass->capture(ctx->display,on_image_snapshoot, img, TRUE);
            // return for timeout status
            switch(status) {
                case Status::reinit:
                case Status::error:
                case Status::ok:
                case Status::timeout:
                return ec != Status::ok ? ec : status;
            }
        }

        return Status::ok;
    }

    void captureThreadSync(CaptureThreadSyncContext* ctx) {
        // start capture thread sync thread and create a reference to its context
        platf::Display* disp;
        Encoder* encoder = NVENC;

        util::ListObject* synced_session_ctxs = LIST_OBJECT_CLASS->init();

        // display selection
        {
            char* chosen_display  = NULL;
            char** display_names  = platf::display_names(map_dev_type(encoder->dev_type));

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
            while(ctx->sync_session_ctx_queue) {
                disp = reset_display(encoder->dev_type, chosen_display, ENCODER_CONFIG->framerate);
                if(disp) 
                    break;

                std::this_thread::sleep_for(200ms);
            }

            // fail to get display names
            if(!disp) {
                LOG_ERROR("unable to create display");
                goto done;
            }
        }

        // TODO: foreach instead
        {
            while (!QUEUE_ARRAY_CLASS->peek(ctx->sync_session_ctx_queue)) { }

            util::Object* obj = QUEUE_ARRAY_CLASS->pop(ctx->sync_session_ctx_queue);
            if (!obj->data)
                goto done;
            
            ((SyncSessionContext*)obj->data)->encoder = encoder;
            ((SyncSessionContext*)obj->data)->display = disp;
            QUEUE_ARRAY_CLASS->push(ctx->sync_session_ctx_queue,obj);
        }

        // run encoder in infinite loop
        while(encode_run_sync(synced_session_ctxs, ctx->sync_session_ctx_queue) == Status::reinit) {}

        done:
        QUEUE_ARRAY_CLASS->stop(ctx->sync_session_ctx_queue);
        
        while (LIST_OBJECT_CLASS->has_data(synced_session_ctxs,0))
        {
            util::Object* obj = LIST_OBJECT_CLASS->get_data(synced_session_ctxs,0);
            SyncSessionContext* ss_ctx = (SyncSessionContext*) OBJECT_CLASS->ref(obj);

            RAISE_EVENT(ss_ctx->shutdown_event);
            RAISE_EVENT(ss_ctx->join_event);
            OBJECT_CLASS->unref(obj);
        }

        while (QUEUE_ARRAY_CLASS->peek(ctx->sync_session_ctx_queue))
        {
            util::Object* obj = QUEUE_ARRAY_CLASS->pop(ctx->sync_session_ctx_queue);
            SyncSessionContext* ss_ctx = (SyncSessionContext*) OBJECT_CLASS->ref(obj);
            RAISE_EVENT(ss_ctx->shutdown_event);
            RAISE_EVENT(ss_ctx->join_event);
            OBJECT_CLASS->unref(obj);
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
    capture( util::Broadcaster* shutdown_event,
             util::QueueArray* packet_queue,
             Config config,
             pointer data) 
    {
        util::Broadcaster* join_event = NEW_EVENT;
        util::Broadcaster* idr_event = NEW_EVENT;
        RAISE_EVENT(idr_event);

        // start capture thread sync and let it manages its own context
        CaptureThreadSyncContext ctx = {0};
        ctx.sync_session_ctx_queue = QUEUE_ARRAY_CLASS->init();
        captureThreadSync(&ctx);

        // push new session context to concode queue
        SyncSessionContext ss_ctx = {0};
        ss_ctx.shutdown_event = shutdown_event;
        ss_ctx.idr_event = idr_event;
        ss_ctx.join_event = join_event;
        ss_ctx.packet_queue = packet_queue;
        ss_ctx.frame_nr = 1;

        // TODO
        // ss_ctx.config = ;
        ss_ctx.channel_data = data;

        util::Object* obj = OBJECT_CLASS->init(&ss_ctx,sizeof(SyncSessionContext),DO_NOTHING);
        QUEUE_ARRAY_CLASS->push(ctx.sync_session_ctx_queue,obj);

        // Wait for join signal
        WAIT_EVENT(join_event);
    }


    platf::PixelFormat
    map_pix_fmt(libav::PixelFormat fmt) 
    {
        switch(fmt) {
            case AV_PIX_FMT_YUV420P10:
                return platf::PixelFormat::yuv420p10;
            case AV_PIX_FMT_YUV420P:
                return platf::PixelFormat::yuv420p;
            case AV_PIX_FMT_NV12:
                return platf::PixelFormat::nv12;
            case AV_PIX_FMT_P010:
                return platf::PixelFormat::p010;
            default:
                return platf::PixelFormat::unknown_pixelformat;
        }

        return platf::PixelFormat::unknown_pixelformat;
    }


    int 
    validate_config(platf::Display* disp, 
                    Encoder* encoder, 
                    Config* config) 
    {
        disp = reset_display( encoder->dev_type, ENCODER_CONFIG->output_name, config->framerate);
        if(!disp) {
            return -1;
        }

        platf::PixelFormat pix_fmt  = config->dynamicRange == 0 ? 
                                        map_pix_fmt(encoder->static_pix_fmt) : 
                                        map_pix_fmt(encoder->dynamic_pix_fmt);

        platf::HWDevice* hwdevice = disp->klass->make_hwdevice(disp,pix_fmt);
        if(!hwdevice) {
            return -1;
        }

        Session* session = NULL;
        make_session(session, encoder, config, disp->width, disp->height, hwdevice);
        if(!session) {
            return -1;
        }

        platf::Image* img = disp->klass->alloc_img(disp);

        if(!img || disp->klass->dummy_img(disp,img)) 
            return -1;
        
        if(session->device->klass->convert(session->device,img)) 
            return -1;
        

        libav::Frame* frame = session->device->frame;
        frame->pict_type = AV_PICTURE_TYPE_I;

        util::QueueArray* packets = QUEUE_ARRAY_CLASS->init();
        while(!QUEUE_ARRAY_CLASS->peek(packets)) {
            if(encode(1, session, frame, packets, NULL)) {
                return -1;
            }
        }

        util::Object* obj = QUEUE_ARRAY_CLASS->pop(packets);
        Packet* packet    = (Packet*)OBJECT_CLASS->ref(obj);

        libav::Packet* av_packet = packet->packet;
        OBJECT_CLASS->unref(obj);

        if(!(av_packet->flags & AV_PKT_FLAG_KEY)) {
            // BOOST_LOG(error) << "First packet type is not an IDR frame"sv;
            return -1;
        }

        int flag = 0;
        if(bitstream::validate_sps(av_packet, config->videoFormat ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264)) {
            flag |= ValidateFlags::VUI_PARAMS;
        }

        char* hevc_nalu = "\000\000\000\001(";
        char* h264_nalu = "\000\000\000\001e";
        char* nalu_prefix = config->videoFormat ? hevc_nalu : h264_nalu;
        std::string_view payload { (char *)av_packet->data, (std::size_t)av_packet->size };

        // TODO search
        // if(std::search(std::begin(payload), std::end(payload), std::begin(nalu_prefix), std::end(nalu_prefix)) != std::end(payload)) 
        //     flag |= ValidateFlags::NALU_PREFIX_5b;

        return flag;
    }
}