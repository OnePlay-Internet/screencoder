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
#include <sunshine_array.h>
#include <common.h>
#include <display.h>



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
        event::Broadcaster* shutdown_event;
        event::Broadcaster* idr_event;
        event::Broadcaster* join_event;

        util::QueueArray* packet_queue;

        int frame_nr;
        Config config;
        pointer channel_data;
    }SyncSessionContext;

    typedef struct SyncSession{
        SyncSessionContext* ctx;
        platf::Image* img_tmp;
        Session session;
    }SyncSession;

    typedef struct _Session {
        libav::CodecContext* context;
        platf::HWDevice* device;

        /**
         * @brief 
         * Replace
         */
        ArrayObject* replacement_array;

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


    PacketClass*
    packet_class_init()
    {
        static PacketClass klass;
        return &klass;
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

        libav::CodecContext* ctx = session->ctx;

        bitstream::NAL sps = session->sps;
        bitstream::NAL vps = session->vps;

        /* send the frame to the encoder */
        auto ret = avcodec_send_frame(ctx, frame);
        if(ret < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            // BOOST_LOG(error) << "Could not send a frame for encoding: "sv << av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
            return -1;
        }

        while(ret >= 0) {
            Packet* packet    = packet_class_init()->init();
            libav::Packet* av_packet = packet->packet;

            ret = avcodec_receive_packet(ctx, av_packet);

            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
                return 0;
            else if(ret < 0) 
                return ret;

            if(session->inject) {
                if(session->inject == 1) {
                    bitstream::H264 h264 = bitstream::make_sps_h264(ctx, av_packet);
                    sps = h264.sps;
                } else {
                    bitstream::HEVC hevc = bitstream::make_sps_hevc(ctx, av_packet);

                    sps = hevc.sps;
                    vps = hevc.vps;

                    array_object_emplace_back(
                    std::string_view((char *)std::begin(vps.old), vps.old.size()),
                    std::string_view((char *)std::begin(vps._new), vps._new.size()));
                }

                session->inject = 0;


                Replace* replace = malloc(sizeof(Replace));
                replace->old = sps.old;
                replace->_new = sps._new;
                array_object_emplace_back(session->replacement_array,replace);
            }

            packet->replacement_array = session->replacement_array;
            packet->channel_data = channel_data;

            object::Object* object = OBJECT_CLASS->init(packet,DO_NOTHING);
            QUEUE_ARRAY_CLASS->push(packets,object);
        }

        return 0;
    }


    Session*
    make_session(const Encoder* encoder, 
                 const Config config, 
                 int width, int height, 
                 platf::HWDevice* hwdevice) 
    {
        bool hardware = encoder.dev_type != AV_HWDEVICE_TYPE_NONE;

        Profile video_format = config.videoFormat == 0 ? encoder->h264 : encoder->hevc;
        if(!video_format[FrameFlags::PASSED]) {
            // BOOST_LOG(error) << encoder.name << ": "sv << video_format.name << " mode not supported"sv;
            return NULL;
        }

        if(config.dynamicRange && !video_format[FrameFlags::DYNAMIC_RANGE]) {
            // BOOST_LOG(error) << video_format.name << ": dynamic range not supported"sv;
            return NULL;
        }

        AVCodec* codec = avcodec_find_encoder_by_name(video_format.name);
        if(!codec) {
            // BOOST_LOG(error) << "Couldn't open ["sv << video_format.name << ']';
            return NULL;
        }

        libav::CodecContext* ctx = avcodec_alloc_context3(codec);
        ctx->width     = config.width;
        ctx->height    = config.height;
        ctx->time_base = AVRational { 1, config.framerate };
        ctx->framerate = AVRational { config.framerate, 1 };

        if(config.videoFormat == 0) {
            ctx->profile = encoder.profile.h264_high;
        }
        else if(config.dynamicRange == 0) {
            ctx->profile = encoder.profile.hevc_main;
        }
        else {
            ctx->profile = encoder.profile.hevc_main_10;
        }

        // B-frames delay decoder output, so never use them
        ctx->max_b_frames = 0;

        // Use an infinite GOP length since I-frames are generated on demand
        ctx->gop_size = encoder.flags & LIMITED_GOP_SIZE ? INT16_MAX : INT_MAX;
        ctx->keyint_min = INT_MAX;

        if(config.numRefFrames == 0) {
            ctx->refs = video_format[FrameFlags::REF_FRAMES_AUTOSELECT] ? 0 : 16;
        }
        else {
            // Some client decoders have limits on the number of reference frames
            ctx->refs = video_format[FrameFlags::REF_FRAMES_RESTRICT] ? config.numRefFrames : 0;
        }

        ctx->flags |= (AV_CODEC_FLAG_CLOSED_GOP | AV_CODEC_FLAG_LOW_DELAY);
        ctx->flags2 |= AV_CODEC_FLAG2_FAST;

        ctx->color_range = (config.encoderCscMode & 0x1) ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

        int sws_color_space;
        switch(config.encoderCscMode >> 1) {
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
        // BOOST_LOG(info) << "Color range: ["sv << ((config.encoderCscMode & 0x1) ? "JPEG"sv : "MPEG"sv) << ']';

        AVPixelFormat sw_fmt;
        if(config.dynamicRange == 0) {
            sw_fmt = encoder.static_pix_fmt;
        }
        else {
            sw_fmt = encoder.dynamic_pix_fmt;
        }

        // Used by cbs::make_sps_hevc
        ctx->sw_pix_fmt = sw_fmt;

        buffer_t hwdevice_ctx;
        if(hardware) {
            ctx->pix_fmt = encoder.dev_pix_fmt;

            auto buf_or_error = encoder.make_hwdevice_ctx(hwdevice.get());
            if(buf_or_error.has_right()) {
            return NULL;
            }

            hwdevice_ctx = std::move(buf_or_error.left());
            if(hwframe_ctx(ctx, hwdevice_ctx, sw_fmt)) {
            return NULL;
            }

            ctx->slices = config.slicesPerFrame;
        }
        else /* software */ {
            ctx->pix_fmt = sw_fmt;

            // Clients will request for the fewest slices per frame to get the
            // most efficient encode, but we may want to provide more slices than
            // requested to ensure we have enough parallelism for good performance.
            ctx->slices = std::max(config.slicesPerFrame, config::video.min_threads);
        }

        if(!video_format[FrameFlags::SLICE]) {
            ctx->slices = 1;
        }

        ctx->thread_type  = FF_THREAD_SLICE;
        ctx->thread_count = ctx->slices;

        AVDictionary *options { nullptr };
        auto handle_option = [&options](const FrameFlags::option_t &option) {
            std::visit(
            util::overloaded {
                [&](int v) { av_dict_set_int(&options, option.name.c_str(), v, 0); },
                [&](int *v) { av_dict_set_int(&options, option.name.c_str(), *v, 0); },
                [&](std::optional<int> *v) { if(*v) av_dict_set_int(&options, option.name.c_str(), **v, 0); },
                [&](const std::string &v) { av_dict_set(&options, option.name.c_str(), v.c_str(), 0); },
                [&](std::string *v) { if(!v->empty()) av_dict_set(&options, option.name.c_str(), v->c_str(), 0); } },
            option.value);
        };

        for(auto &option : video_format.options) {
            handle_option(option);
        }

        if(video_format[FrameFlags::CBR]) {
            auto bitrate        = config.bitrate * (hardware ? 1000 : 800); // software bitrate overshoots by ~20%
            ctx->rc_max_rate    = bitrate;
            ctx->rc_buffer_size = bitrate / 10;
            ctx->bit_rate       = bitrate;
            ctx->rc_min_rate    = bitrate;
        }
        else if(video_format.qp) {
            handle_option(*video_format.qp);
        }
        else {
            BOOST_LOG(error) << "Couldn't set video quality: encoder "sv << encoder.name << " doesn't support qp"sv;
            return NULL;
        }

        if(auto status = avcodec_open2(ctx, codec, &options)) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            BOOST_LOG(error)
            << "Could not open codec ["sv
            << video_format.name << "]: "sv
            << av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, status);

            return NULL;
        }

        libav::Frame* frame = av_frame_alloc();
        frame->format = ctx->pix_fmt;
        frame->width  = ctx->width;
        frame->height = ctx->height;


        if(hardware) {
            frame->hw_frames_ctx = av_buffer_ref(ctx->hw_frames_ctx);
        }

        platf::HWDevice* device;
        if(!hwdevice->data) {
            // TODO software encoder
            auto device_tmp = std::make_unique<swdevice_t>();

            if(device_tmp->init(width, height, frame.get(), sw_fmt)) {
            return NULL;
            }

            device = std::move(device_tmp);
        }
        else 
        {
            device = hwdevice;
        }

        if(device->set_frame(frame)) {
            return NULL;
        }

        device->set_colorspace(sws_color_space, ctx->color_range);

        Session* session = malloc(sizeof(Session))
        session->ctx = ctx;
        session->device = device;
        // 0 ==> don't inject, 1 ==> inject for h264, 2 ==> inject for hevc
        // session->inject = (1 - (int)video_format[FrameFlags::VUI_PARAMETERS]) * (1 + config.videoFormat),

        // TODO
        // if(!video_format[FrameFlags::NALU_PREFIX_5b]) 
        {
            char* hevc_nalu = "\000\000\000\001(";
            char* h264_nalu = "\000\000\000\001e";
            char* nalu_prefix = config.videoFormat ? hevc_nalu : h264_nalu;
            session.replacements.emplace_back(nalu_prefix.substr(1), nalu_prefix);
        }

        return std::make_optional(std::move(session));
    }


    SyncSession* 
    make_synced_session(platf::Display* disp, 
                        const Encoder* encoder, 
                        platf::Image *img, 
                        SyncSessionContext *ctx) 
    {
        SyncSession* encode_session = (SyncSession*)malloc(sizeof(SyncSession));

        encode_session->ctx = ctx;

        platf::PixelFormat pix_fmt = ctx.config.dynamicRange == 0 ? map_pix_fmt(encoder.static_pix_fmt) : map_pix_fmt(encoder.dynamic_pix_fmt);
        auto hwdevice = disp->make_hwdevice(pix_fmt);

        if(!hwdevice) {
            return NULL;
        }

        // absolute mouse coordinates require that the dimensions of the screen are known
        ctx.touch_port_events->raise(make_port(disp, ctx.config));

        auto session = make_session(encoder, ctx.config, img.width, img.height, std::move(hwdevice));
        if(!session) {
            return NULL;
        }

        encode_session.session = *session;

        return encode_session;
    }



    /**
     * @brief 
     * 
     * @param img 
     * @param synced_session_ctxs 
     * @param encode_session_ctx_queue 
     */
    void 
    on_image_snapshoot (platf::Image* img,
                        ArrayObject* synced_sessions,
                        ArrayObject* synced_session_ctxs,
                        util::QueueArray* encode_session_ctx_queue)
    {
        while(QUEUE_ARRAY_CLASS->peek(encode_session_ctx_queue)) {

            SyncSessionContext* encode_session_ctx = encode_session_ctx_queue.pop();
            if(!encode_session_ctx) {
                return NULL;
            }

            array_object_emplace_back(synced_session_ctxs,encode_session_ctx);

            SyncSession* encode_session = make_synced_session(disp.get(), encoder, *img, *synced_session_ctxs.back());
            if(!encode_session) {
            ec = Status::error;
            return nullptr;
            }

            array_object_emplace_back(synced_sessions,encode_session);
        }

        while (/* condition */)
        {
        // KITTY_WHILE_LOOP(auto pos = std::begin(synced_sessions), pos != std::end(synced_sessions), {

            // get frame from device
            auto frame = pos->session.device->frame;

            // get avcodec context from synced session
            auto ctx   = pos->ctx;

            // shutdown while loop whenever shutdown event happen
            if(ctx->shutdown_event->peek()) {
            // Let waiting thread know it can delete shutdown_event
            ctx->join_event->raise(true);

            pos = synced_sessions.erase(pos);
            synced_session_ctxs.erase(std::find_if(std::begin(synced_session_ctxs), std::end(synced_session_ctxs), [&ctx_p = ctx](auto &ctx) {
                return ctx.get() == ctx_p;
            }));

            if(synced_sessions.empty()) {
                return nullptr;
            }

            continue;
            }

            // peek idr event (keyframe)
            if(ctx->idr_events->peek()) {
            frame->pict_type = AV_PICTURE_TYPE_I;
            frame->key_frame = 1;

            ctx->idr_events->pop();
            }

            //setup hardware device context with base image profile
            if(pos->session.device->convert(*img)) {
            BOOST_LOG(error) << "Could not convert image"sv;
            ctx->shutdown_event->raise(true);

            continue;
            }

            // encode
            if(encode(ctx->frame_nr++, pos->session, frame, ctx->packets, ctx->channel_data)) {
                LOG_ERROR("Could not encode video packet");
                ctx->shutdown_event->raise(true);
                continue;

            }

            // reset keyframe attribute
            frame->pict_type = AV_PICTURE_TYPE_NONE;
            frame->key_frame = 0;

            ++pos;
        };
        return img;
    }


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

        // pop context from context queue and move them to synced session queue
        if(synced_session_ctxs.empty()) {
            object::Object* obj = OBJECT_CLASS->init(NULL,DO_NOTHING);
            QUEUE_ARRAY_CLASS->pop(encode_session_ctx_queue,obj);
            SyncSessionContext* ctx = (SyncSessionContext*) obj->data;
            if(!ctx) 
                return Status::ok;

            synced_session_ctxs.emplace_back(std::make_unique<sync_session_ctx_t>(std::move(*ctx)));
        }

        // read framerate from synced session 
        int framerate = synced_session_ctxs.front()->config.framerate;

        // reset display every 200ms until display is ready
        while(encode_session_ctx_queue.running()) {
            reset_display(disp, encoder.dev_type, display_names[display_p], framerate);
            if(disp) 
                break;
            std::this_thread::sleep_for(200ms);
        }

        if(!disp) {
            return encoder::error;
        }

        // allocate display image and intialize with dummy data
        auto img = disp->alloc_img();
        if(!img || disp->dummy_img(img.get())) {
            return Status::error;
        }

        // create one session for easch synced session context
        SyncSessionContext* synced_sessions;
        for(auto &ctx : synced_session_ctxs) {
            auto synced_session = make_synced_session(disp.get(), encoder, *img, *ctx);
            if(!synced_session) {
            return Status::error;
            }

            synced_sessions.emplace_back(std::move(*synced_session));
        }


        // return status
        auto ec = Status::ok;
        while(encode_session_ctx_queue.running()) {
            // run image capture in while loop
            auto status = ->capture(disp,on_image_snapshoot, img, &display_cursor);
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
                return platf::PixelFormat::unknown;
        }

        return platf::PixelFormat::unknown;
    }
}