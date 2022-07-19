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

#include <encoder_packet.h>
#include <encoder_validate.h>
#include <encoder_d3d11_device.h>
#include <sunshine_config.h>

#include <platform_common.h>
#include <display.h>
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

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

        /**
         * @brief 
         * encoder::Session
         */
        util::Buffer* session;
    }SyncSession;







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
           SyncSession* sync_session, 
           libav::Frame* frame, 
           util::QueueArray* packets, 
           void *channel_data) 
    {
        Session* session = (Session*)BUFFER_CLASS->ref(sync_session->session,NULL);
        libav::CodecContext* libav_ctx = session->context;
        frame->pts = (int64_t)frame_nr;

        /* send the frame to the encoder */
        auto ret = avcodec_send_frame(libav_ctx, frame);
        if(ret < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret));
            return -1;
        }


        Packet* packet    = packet_class_init()->init();
        libav::Packet* av_packet = (libav::Packet*)BUFFER_CLASS->ref(packet->packet,NULL);

        ret = avcodec_receive_packet(libav_ctx, av_packet);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
            return 0;
        else if(ret < 0) 
            return ret;

        if(session->inject) {
            if(session->inject == 1) {
                //H264
            } else {
                //H265
            }
        }


        // we pass the reference of session to packet
        packet->session = sync_session->session;
        packet->user_data = channel_data;

        util::Buffer* obj = BUFFER_CLASS->init(packet,sizeof(Packet),packet_class_init()->finalize);
        QUEUE_ARRAY_CLASS->push(packets,obj);

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

    libav::Stream*
    rtp_avformat_new_stream(libav::FormatContext* ctx, 
                            libav::Codec* codec) 
    {
        libav::Stream *st = NULL;

        if(codec == NULL)
            return NULL;

        st = avformat_new_stream(ctx, codec);
        if(!st)
            return NULL;

        // format specific index
        st->id = 0;
        if(ctx->flags & AVFMT_GLOBALHEADER) {
            st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
            
        // should we always set global header?
        if(codec->id == AV_CODEC_ID_H264) 
            st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

        return st;
    }

    Session*
    make_session(Encoder* encoder, 
                 Config* config, 
                 int width, int height, 
                 platf::HWDevice* hwdevice) 
    {
        Session* session = (Session*)malloc(sizeof(Session));
        memset(session,0,sizeof(Session));

        bool hardware = encoder->dev_type != AV_HWDEVICE_TYPE_NONE;

        CodecConfig* video_format = (config->videoFormat == 0) ? &encoder->h264 : &encoder->hevc;
        if(!video_format->capabilities[FrameFlags::PASSED]) {
            LOG_ERROR("encoder not supported");
            goto failed;
        }

        if(config->dynamicRange && !video_format->capabilities[FrameFlags::DYNAMIC_RANGE]) {
            LOG_ERROR("dynamic range not supported");
            goto failed;
        }

        libav::Codec* codec = avcodec_find_encoder_by_name(video_format->name);
        if(!codec) {
            LOG_ERROR("Couldn't create encoder");
            goto failed;
        }
        libav::Format* format = av_guess_format("rtp", NULL, NULL);
        if(!format) {
            LOG_ERROR("Couldn't create rtppay");
            goto failed;
        }

        libav::FormatContext* fmtctx = avformat_alloc_context();
        fmtctx->oformat = format;

		if(fmtctx->packet_size > 0) 
			fmtctx->packet_size = MAX(ENCODER_CONFIG->mtu,fmtctx->packet_size);
		else 
			fmtctx->packet_size = ctx->mtu;
		

        libav::Stream* stream = rtp_avformat_new_stream(fmtctx,codec);
        libav::CodecContext* ctx = stream->codec;
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
            if(!buf_or_error)
                goto failed;

            hwdevice_ctx = buf_or_error;
            if(hwframe_ctx(ctx, hwdevice_ctx, sw_fmt) != 0) 
                goto failed;
            
            ctx->slices = config->slicesPerFrame;
        } else /* software */ {
            // TODO software 
            // ctx->pix_fmt = sw_fmt;

            // // Clients will request for the fewest slices per frame to get the
            // // most efficient encode, but we may want to provide more slices than
            // // requested to ensure we have enough parallelism for good performance.
            // ctx->slices = MAX(config->slicesPerFrame, ENCODER_CONFIG->min_threads);
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
            goto failed;
        }

        if(auto status = avcodec_open2(ctx, codec, &options)) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            // BOOST_LOG(error)
            // << "Could not open codec ["sv
            // << video_format->name << "]: "sv
            // << av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, status);
            goto failed;
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
            goto failed;
        

        device->klass->set_colorspace(device,sws_color_space, ctx->color_range);

        session->pts = frame->ptx;
        session->format_context = fmtctx;
        session->stream = stream;
        session->context = ctx;
        session->device = device;


        // 0 ==> don't inject, 1 ==> inject for h264, 2 ==> inject for hevc
        // if VUI param are set, then no need to inject
        // if VUI param are not set, then set inject option based on video format
        if(!(bool)video_format->capabilities[FrameFlags::VUI_PARAMETERS])
            if(config->videoFormat)
                session->inject = 2;
            else
                session->inject = 1;
        else
            session->inject = 0;

        return session;
    
    failed:
        free(session);
        return NULL;
    }

    void
    session_finalize(pointer session)
    {
        free(session);

    }

    /**
     * @brief 
     * 
     * @param self 
     * @param img 
     * @param ctx 
     */
    bool
    make_synced_session(SyncSession* self,
                        platf::Image *img, 
                        SyncSessionContext *ctx) 
    {
        platf::PixelFormat pix_fmt = ctx->config->dynamicRange == 0 ? 
                                        map_pix_fmt(ctx->encoder->static_pix_fmt) : 
                                        map_pix_fmt(ctx->encoder->dynamic_pix_fmt);

        platf::HWDevice* hwdevice = ctx->display->klass->make_hwdevice(ctx->display,pix_fmt);

        if(!hwdevice) 
            return FALSE;

        Session* ses = make_session(ctx->encoder,ctx->config, img->width, img->height, hwdevice);
        if(!ses)
            return FALSE;

        self->session = BUFFER_CLASS->init((pointer)ses,sizeof(Session),session_finalize);
        self->ctx = ctx;
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
    on_image_snapshoot (platf::Image* img,
                        SyncSession* syncsession)
    {
        Session* session = (Session*)BUFFER_CLASS->ref(syncsession,NULL);
        // get frame from device
        libav::Frame* frame = session->device->frame;

        // get avcodec context from synced session
        SyncSessionContext* ctx   = syncsession->ctx;

        // shutdown while loop whenever shutdown event happen
        if(IS_INVOKED(ctx->shutdown_event)) {
            // Let waiting thread know it can delete shutdown_event
            RAISE_EVENT(ctx->join_event);
            return platf::Capture::error;
        }

        // peek idr event (keyframe)
        if(IS_INVOKED(ctx->idr_event)) {
            frame->pict_type = AV_PICTURE_TYPE_I;
            frame->key_frame = 1;
        }

        // convert image
        if(session->device->klass->convert(session->device,img)) {
            LOG_ERROR("Could not convert image");
            RAISE_EVENT(ctx->shutdown_event);
            continue;
        }

        // encode
        if(encode(ctx->frame_nr++, 
                    syncsession, 
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
    encode_run_sync(SyncSessionContext* ctx) 
    {
        SyncSession synced_session = {0};

        // allocate display image and intialize with dummy data
        platf::Image* img = ctx->display->klass->alloc_img(ctx->display);
        if(!img || ctx->display->klass->dummy_img(ctx->display,img)) 
            return Status::error;

        if(!make_synced_session(synced_session,img, ctx))
            return Status::error;

        // cursor
        // run image capture in while loop, 
        return (Status)ctx->display->klass->capture(ctx->display, img,on_image_snapshoot, synced_session, TRUE);
    }

    /**
     * @brief 
     * 
     * @param ctx 
     */
    void 
    captureThreadSync(SyncSessionContext* ctx) 
    {
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

        ctx->display = disp;
        ctx->encoder = encoder;


        // run encoder in infinite loop
        while(encode_run_sync(ctx) == Status::reinit) {}

        done:
        QUEUE_ARRAY_CLASS->stop(ctx->sync_session_ctx_queue);
        
        while (LIST_OBJECT_CLASS->has_data(synced_session_ctxs,0))
        {
            util::Buffer* obj = LIST_OBJECT_CLASS->get_data(synced_session_ctxs,0);

            int size;
            SyncSessionContext* ss_ctx = (SyncSessionContext*) BUFFER_CLASS->ref(obj,&size);
            if(size != sizeof(SyncSessionContext)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            RAISE_EVENT(ss_ctx->shutdown_event);
            RAISE_EVENT(ss_ctx->join_event);
            BUFFER_CLASS->unref(obj);
        }

        while (QUEUE_ARRAY_CLASS->peek(ctx->sync_session_ctx_queue))
        {
            util::Buffer* obj = QUEUE_ARRAY_CLASS->pop(ctx->sync_session_ctx_queue);

            int size;
            SyncSessionContext* ss_ctx = (SyncSessionContext*) BUFFER_CLASS->ref(obj,&size);
            if(size != sizeof(SyncSessionContext)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            RAISE_EVENT(ss_ctx->shutdown_event);
            RAISE_EVENT(ss_ctx->join_event);
            BUFFER_CLASS->unref(obj);
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

        // push new session context to concode queue
        SyncSessionContext ss_ctx = {0};
        ss_ctx.shutdown_event = shutdown_event;
        ss_ctx.idr_event = idr_event;
        ss_ctx.join_event = join_event;
        ss_ctx.packet_queue = packet_queue;
        ss_ctx.frame_nr = 1;

        ss_ctx.config = &config;
        ss_ctx.channel_data = data;
        captureThreadSync(&ss_ctx);

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

        util::Buffer* obj = QUEUE_ARRAY_CLASS->pop(packets);

        int size;
        Packet* packet    = (Packet*)BUFFER_CLASS->ref(obj,&size);
        if(size != sizeof(Packet)) {
            LOG_ERROR("wrong datatype");
            return -1;
        }

        util::Buffer* av_buffer = packet->packet;
        BUFFER_CLASS->unref(obj);

        libav::Packet* av_packet = (libav::Packet*)BUFFER_CLASS->ref(av_buffer,&size);
        if (size != sizeof(libav::Packet))
        {
            LOG_ERROR("wrong datatype");
            return -1;
        }
        


        if(!(av_packet->flags & AV_PKT_FLAG_KEY)) {
            // BOOST_LOG(error) << "First packet type is not an IDR frame"sv;
            return -1;
        }

        int flag = 0;
        if(bitstream::validate_packet(av_packet, config->videoFormat ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264)) {
            flag |= ValidateFlags::VUI_PARAMS;
        }

        char* hevc_nalu = "\000\000\000\001(";
        char* h264_nalu = "\000\000\000\001e";
        char* nalu_prefix = config->videoFormat ? hevc_nalu : h264_nalu;
        util::Buffer* payload = BUFFER_CLASS->init(av_packet->data,av_packet->size,DO_NOTHING);
        util::Buffer* pref =  BUFFER_CLASS->init(nalu_prefix,strlen(nalu_prefix),DO_NOTHING);

        if(BUFFER_CLASS->search(payload,pref) != BUFFER_CLASS->size(payload))
            flag |= ValidateFlags::NALU_PREFIX_5B;

        BUFFER_CLASS->unref(av_buffer);
        BUFFER_CLASS->unref(payload);
        BUFFER_CLASS->unref(pref);
        return flag;
    }
}