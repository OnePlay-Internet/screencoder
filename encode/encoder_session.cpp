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
    handle_options(AVDictionary** options, 
                   util::KeyValue* keyvalue)
    {
        util::KeyValue* option = keyvalue;
        while(option->type) {
            if(option->type == util::Type::STRING)
                av_dict_set(options,option->key,option->string_value,0);

            if(option->type == util::Type::INT)
                av_dict_set_int(options,option->key,option->int_value,0);

            option++;
        }
    }


    void
    free_encode_context(pointer data)
    {
        EncodeContext* ctx = (EncodeContext*)data;
        avcodec_free_context(&ctx->context);
        ctx->device->klass->finalize(ctx->device);
        free(data);
    }

    EncodeContext*
    alloc_encode_context(platf::Device* device,
                        char* encoder)
    {
        EncodeContext* ctx = (EncodeContext*)malloc(sizeof(EncodeContext));
        memset((pointer)ctx,0,sizeof(EncodeContext));


        ctx->device = device;
        ctx->codec = (libav::Codec*)avcodec_find_encoder_by_name(encoder);
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


    EncodeContext*
    make_encode_context(Encoder* encoder, 
                        Config* config,
                        int width, int height, int framerate, 
                        platf::Device* device) 
    {
        CodecConfig* video_format = encoder->codec_config;
        if(!video_format->capabilities[FrameFlags::PASSED]) {
            LOG_ERROR("encoder not supported");
            return NULL;
        }

        if(config->dynamicRangeOption == encoder::DynamicRange::ENABLE &&
          !video_format->capabilities[FrameFlags::DYNAMIC_RANGE]) {
            LOG_ERROR("dynamic range not supported");
            return NULL;
        }

        char log[50] = {0};
        snprintf(log,50,"starting with encoder %s",video_format->name);
        LOG_INFO(log);
        EncodeContext* encode_ctx = alloc_encode_context(device,video_format->name);
        encode_ctx->dev_type = encoder->dev_type;

        libav::CodecContext* ctx = encode_ctx->context;
        {
            ctx->width     = width;
            ctx->height    = height;
            ctx->time_base = AVRational { 1, framerate };
            ctx->framerate = AVRational { framerate, 1 };

            if(encoder->codec_config->format == VideoFormat::H264) {
                ctx->profile = encoder->profile.h264_high;
            } else if(config->dynamicRangeOption == DynamicRange::ENABLE) {
                ctx->profile = encoder->profile.hevc_main_10;
            } else {
                ctx->profile = encoder->profile.hevc_main;
            }

            // B-frames delay decoder output, so never use them
            ctx->max_b_frames = 0;

            // if gop_size are set then 
            ctx->gop_size =    SCREENCODER_CONSTANT->gop_size;
            ctx->keyint_min =  SCREENCODER_CONSTANT->gop_size_min;
            ctx->refs =        SCREENCODER_CONSTANT->ref_frame_num;

            ctx->flags  |= (AV_CODEC_FLAG_CLOSED_GOP | AV_CODEC_FLAG_LOW_DELAY);
            ctx->flags2 |= AV_CODEC_FLAG2_FAST;

            ctx->color_range = config->avcolor;
            switch(config->scalecolor) {
            case LibscaleColor::REC_601:
            default:
                // Rec. 601
                LOG_INFO("Color coding [Rec. 601]");
                ctx->color_primaries = AVCOL_PRI_SMPTE170M;
                ctx->color_trc       = AVCOL_TRC_SMPTE170M;
                ctx->colorspace      = AVCOL_SPC_SMPTE170M;
                encode_ctx->sws_color_space      = SWS_CS_SMPTE170M;
                break;
            case LibscaleColor::REC_709:
                // Rec. 709
                LOG_INFO("Color coding [Rec. 709]");
                ctx->color_primaries = AVCOL_PRI_BT709;
                ctx->color_trc       = AVCOL_TRC_BT709;
                ctx->colorspace      = AVCOL_SPC_BT709;
                encode_ctx->sws_color_space      = SWS_CS_ITU709;
                break;
            case LibscaleColor::REC_2020:
                // Rec. 2020
                LOG_INFO("Color coding [Rec. 2020]");
                ctx->color_primaries = AVCOL_PRI_BT2020;
                ctx->color_trc       = AVCOL_TRC_BT2020_10;
                ctx->colorspace      = AVCOL_SPC_BT2020_NCL;
                encode_ctx->sws_color_space      = SWS_CS_BT2020;
                break;
            }

            libav::PixelFormat sw_fmt = config->dynamicRangeOption == DynamicRange::ENABLE ?  
                encoder->dynamic_pix_fmt:
                encoder->static_pix_fmt; 

            if(encoder->dev_type != AV_HWDEVICE_TYPE_NONE) {
                ctx->pix_fmt = encoder->dev_pix_fmt;

                libav::BufferRef* buf_or_error = encoder->make_hw_ctx_func(device);
                if(!buf_or_error)
                    return NULL;

                libav::BufferRef* hwdevice_ctx = buf_or_error;
                if(hwframe_ctx(ctx, hwdevice_ctx, sw_fmt) != 0) 
                    return NULL;
                
                ctx->slices = config->slicesPerFrame;
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
        }

        /**
         * @brief 
         * map from config to option here
         */
        {
            AVDictionary *options = NULL;
            handle_options(&options,video_format->options);
            if(video_format->capabilities[FrameFlags::CBR]) {
                ctx->rc_max_rate    = DEFAULT_BITRATE ;
                ctx->rc_buffer_size = DEFAULT_BITRATE / framerate;
                ctx->bit_rate       = DEFAULT_BITRATE ;
                ctx->rc_min_rate    = DEFAULT_BITRATE ;
            } else {
                av_dict_set_int(&options,"qp",50,0);
            }

            if(int status = avcodec_open2(ctx, encode_ctx->codec, &options)) {
                char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
                LOG_ERROR("Could not open codec");
                LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, status));
                return NULL;
            }
        }


        return encode_ctx;
    }


    util::Buffer*
    make_avframe_buffer(EncodeContext* context)
    {
        libav::Frame* frame = av_frame_alloc();
        frame->format = context->context->pix_fmt;
        frame->width  = context->context->width;
        frame->height = context->context->height;

        frame->hw_frames_ctx = (context->dev_type != AV_HWDEVICE_TYPE_NONE) ? 
            av_buffer_ref(context->context->hw_frames_ctx) : 
            frame->hw_frames_ctx;

        return BUFFER_INIT(frame,sizeof(libav::Frame),libav::frame_free_func);
    }




    /**
     * @brief 
     * 
     * @param self 
     * @param img 
     * @param ctx 
     */
    util::Buffer*
    make_encode_context_buffer(Encoder* encoder, 
                                Config* config,
                                platf::Display* display,
                                sink::GenericSink* sink) 
    {
        platf::PixelFormat pix_fmt = !config->dynamicRangeOption == DynamicRange::ENABLE ?
                                        platf::map_pix_fmt(encoder->static_pix_fmt) : 
                                        platf::map_pix_fmt(encoder->dynamic_pix_fmt);

        platf::Device* device = display->klass->make_hwdevice(display,pix_fmt);
        if(!device) {
            NEW_ERROR(error::Error::CREATE_HW_DEVICE_FAILED);
        }

        EncodeContext* context = make_encode_context(encoder, config,
                                    display->width, 
                                    display->height, 
                                    display->framerate,
                                    device);
        if(!context) {
            NEW_ERROR(error::Error::CREATE_SESSION_FAILED);
        }

        device->klass->set_colorspace(device, context->sws_color_space, context->context->color_range);

        sink->preset(sink,context);
        return BUFFER_INIT(context,sizeof(EncodeContext),free_encode_context);
    }
} // namespace encoder
