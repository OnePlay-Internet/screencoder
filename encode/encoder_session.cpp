/**
 * @file encoder_session.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_session.h>
#include <encoder_datatype.h>

#include <screencoder_util.h>
#include <screencoder_rtp.h>
#include <encoder_device.h>

#include <screencoder_config.h>
#include <encoder_thread.h>

extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace encoder
{

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
                libav::BufferRef* device, 
                libav::PixelFormat format) 
    {
        libav::BufferRef* frame_ref = av_hwframe_ctx_alloc(device);

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

    void
    handle_options(AVDictionary* options, 
                   util::KeyValue* keyvalue)
    {
        util::KeyValue* option = keyvalue;
        while(option->type) {
            if(option->type == util::Type::STRING)
                av_dict_set(&options,option->key,option->string_value,0);

            if(option->type == util::Type::INT)
                av_dict_set_int(&options,option->key,option->int_value,0);

            option++;
        }
    }


    void
    free_encode_context(pointer data)
    {
        EncodeContext* ctx = (EncodeContext*)data;
        avcodec_free_context(&ctx->context);
        free(data);
    }

    EncodeContext*
    make_encode_context(platf::Device* device,
                        char* encoder)
    {
        EncodeContext* ctx = (EncodeContext*)malloc(sizeof(EncodeContext));
        memset((pointer)ctx,0,sizeof(EncodeContext));


        ctx->device = device;
        ctx->codec = avcodec_find_encoder_by_name(encoder);
        if(!ctx->codec) {
            LOG_ERROR("Couldn't create encoder");
            return NULL;
        }
        ctx->context = avcodec_alloc_context3(ctx->codec);
        if(!ctx->context) {
            LOG_ERROR("Couldn't create context");
            return NULL;
        }
        
        return ctx;
    }


    Session*
    make_session(Encoder* encoder, 
                 int width, int height, int framerate,
                 platf::Device* device) 
    {
        bool hardware = encoder->dev_type != AV_HWDEVICE_TYPE_NONE;

        CodecConfig* video_format = (encoder->conf.videoFormat == 0) ? &encoder->h264 : &encoder->hevc;
        if(!video_format->capabilities[FrameFlags::PASSED]) {
            LOG_ERROR("encoder not supported");
            return NULL;
        }

        if(encoder->conf.dynamicRange && !video_format->capabilities[FrameFlags::DYNAMIC_RANGE]) {
            LOG_ERROR("dynamic range not supported");
            return NULL;
        }

        EncodeContext* encode_ctx = make_encode_context(device,video_format->name);
        libav::CodecContext* ctx = encode_ctx->context;
        ctx->width     = width;
        ctx->height    = height;
        ctx->time_base = AVRational { 1, framerate };
        ctx->framerate = AVRational { framerate, 1 };

        if(encoder->conf.videoFormat == 0) {
            ctx->profile = encoder->profile.h264_high;
        }
        else if(encoder->conf.dynamicRange == 0) {
            ctx->profile = encoder->profile.hevc_main;
        }
        else {
            ctx->profile = encoder->profile.hevc_main_10;
        }

        // B-frames delay decoder output, so never use them
        ctx->max_b_frames = 0;

        // if gop_size are set then 
        ctx->gop_size = SCREENCODER_CONSTANT->gop_size;
        ctx->keyint_min = SCREENCODER_CONSTANT->gop_size_min;

        if(encoder->conf.numRefFrames == 0) {
            ctx->refs = video_format->capabilities[FrameFlags::REF_FRAMES_AUTOSELECT] ? 0 : 16;
        } else {
            // Some client decoders have limits on the number of reference frames
            ctx->refs = video_format->capabilities[FrameFlags::REF_FRAMES_RESTRICT] ? encoder->conf.numRefFrames : 0;
        }

        ctx->flags  |= (AV_CODEC_FLAG_CLOSED_GOP | AV_CODEC_FLAG_LOW_DELAY);
        ctx->flags2 |= AV_CODEC_FLAG2_FAST;

        ctx->color_range = (encoder->conf.encoderCscMode & 0x1) ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

        int sws_color_space;
        switch(encoder->conf.encoderCscMode >> 1) {
        case 0:
        default:
            // Rec. 601
            LOG_INFO("Color coding [Rec. 601]");
            ctx->color_primaries = AVCOL_PRI_SMPTE170M;
            ctx->color_trc       = AVCOL_TRC_SMPTE170M;
            ctx->colorspace      = AVCOL_SPC_SMPTE170M;
            sws_color_space      = SWS_CS_SMPTE170M;
            break;

        case 1:
            // Rec. 709
            LOG_INFO("Color coding [Rec. 709]");
            ctx->color_primaries = AVCOL_PRI_BT709;
            ctx->color_trc       = AVCOL_TRC_BT709;
            ctx->colorspace      = AVCOL_SPC_BT709;
            sws_color_space      = SWS_CS_ITU709;
            break;

        case 2:
            // Rec. 2020
            LOG_INFO("Color coding [Rec. 2020]");
            ctx->color_primaries = AVCOL_PRI_BT2020;
            ctx->color_trc       = AVCOL_TRC_BT2020_10;
            ctx->colorspace      = AVCOL_SPC_BT2020_NCL;
            sws_color_space      = SWS_CS_BT2020;
            break;
        }

        libav::PixelFormat sw_fmt;
        if(encoder->conf.dynamicRange == 0) {
            sw_fmt = encoder->static_pix_fmt;
        } else {
            sw_fmt = encoder->dynamic_pix_fmt;
        }

        // Used by cbs::make_sps_hevc
        ctx->sw_pix_fmt = sw_fmt;

        libav::BufferRef* hwdevice_ctx;
        if(hardware) {
            ctx->pix_fmt = encoder->dev_pix_fmt;

            libav::BufferRef* buf_or_error = encoder->make_hw_ctx_func(device);
            if(!buf_or_error)
                return NULL;

            hwdevice_ctx = buf_or_error;
            if(hwframe_ctx(ctx, hwdevice_ctx, sw_fmt) != 0) 
                return NULL;
            
            ctx->slices = encoder->conf.slicesPerFrame;
        } else {
            // TODO software 
            // ctx->pix_fmt = sw_fmt;

            // // Clients will request for the fewest slices per frame to get the
            // // most efficient encode, but we may want to provide more slices than
            // // requested to ensure we have enough parallelism for good performance.
            // ctx->slices = MAX(config->slicesPerFrame, SCREENCODER_CONSTANT->min_threads);
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
            ctx->rc_max_rate    = encoder->conf.bitrate;
            ctx->rc_buffer_size = encoder->conf.bitrate / 10;
            ctx->bit_rate       = encoder->conf.bitrate;
            ctx->rc_min_rate    = encoder->conf.bitrate / 2;
        } else if(video_format->qp) {
            handle_options(options,video_format->qp);
        } else {
            LOG_ERROR("Couldn't set video quality");
            return NULL;
        }

        if(int status = avcodec_open2(ctx, encode_ctx->codec, &options)) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR("Could not open codec");
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, status));
            return NULL;
        }

        libav::Frame* frame = av_frame_alloc();
        frame->format = ctx->pix_fmt;
        frame->width  = width;
        frame->height = height;


        if(hardware) 
            frame->hw_frames_ctx = av_buffer_ref(ctx->hw_frames_ctx);
        

        if(device->klass->set_frame(device,frame)) 
            return NULL;

        device->klass->set_colorspace(device,
                                        sws_color_space, 
                                        ctx->color_range);

        Session* session = (Session*)malloc(sizeof(Session));
        memset(session,0,sizeof(Session));
        session->pts = frame->pts;
        session->encode = encode_ctx;
        return session;
    }

    void
    session_finalize(pointer session)
    {
        Session* self = (Session*) session;
        free_encode_context(self->encode);
        free(session);
    }




    /**
     * @brief 
     * 
     * @param self 
     * @param img 
     * @param ctx 
     */
    util::Buffer*
    make_session_buffer(platf::Image* img, 
                        Encoder* encoder,
                        platf::Display* display,
                        sink::GenericSink* sink) 
    {
        platf::PixelFormat pix_fmt = encoder->conf.dynamicRange == 0 ? 
                                        platf::map_pix_fmt(encoder->static_pix_fmt) : 
                                        platf::map_pix_fmt(encoder->dynamic_pix_fmt);

        platf::Device* device = display->klass->make_hwdevice(display,pix_fmt);
        if(!device) 
            return NULL;

        Session* ses = make_session(encoder,
                                    display->width, 
                                    display->height, 
                                    display->framerate,
                                    device);
        if(!ses)
            return NULL;

        sink->preset(sink,ses->encode);
        return BUFFER_CLASS->init((pointer)ses,sizeof(Session),session_finalize);
    }
} // namespace encoder
