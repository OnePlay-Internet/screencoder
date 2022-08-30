/**
 * @file encoder_d3d11_device.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_d3d11_device.h>
#include <screencoder_util.h>

#include <encoder_device.h>
#include <platform_common.h>

#include <screencoder_config.h>

#ifdef _WIN32
extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}
#endif

#include <gpu_hw_device.h>

namespace h264 {
    typedef enum _Profile{
        baseline,
        main,
        high,
        high_444p,
    }Profile;

    encoder::CodecConfig
    get_codec_config()
    {
        util::KeyValue* h264pairs = util::new_keyvalue_pairs(6);
        util::keyval_new_intval(h264pairs,"zerolatency",1);
        util::keyval_new_intval(h264pairs,"preset",SCREENCODER_CONSTANT->nv.rc);
        util::keyval_new_intval(h264pairs,"rc",SCREENCODER_CONSTANT->nv.rc);
        util::keyval_new_intval(h264pairs,"coder",SCREENCODER_CONSTANT->nv.coder);
        return
        {
            "h264_nvenc",
            h264pairs,
        };
    }
} // namespace nv

namespace hevc {
    typedef enum _Profile{
        main,
        main_10,
        rext,
    }Profile;

    encoder::CodecConfig
    get_codec_config()
    {        
        util::KeyValue* hevcpairs = util::new_keyvalue_pairs(5);
        util::keyval_new_intval(hevcpairs,"zerolatency",1);
        // util::keyval_new_intval(hevcpairs,"preset",SCREENCODER_CONSTANT->nv.rc);
        // util::keyval_new_intval(hevcpairs,"rc",SCREENCODER_CONSTANT->nv.rc);
        return {
            "hevc_nvenc",
            hevcpairs,
        };
    }
}

namespace encoder {
    /**
     * @brief 
     * 
     * @param hwdevice_ctx 
     * @return libav::BufferRef* 
     */
    libav::BufferRef* 
    dxgi_make_hwdevice_ctx(platf::Device *hwdevice_ctx) 
    {
        libav::BufferRef* ctx_buf = (libav::BufferRef*)av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        AVD3D11VADeviceContext* ctx = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)ctx_buf->data)->hwctx;
        memset((pointer) ctx, 0, sizeof (AVD3D11VADeviceContext));


        d3d11::Device device = (d3d11::Device)((gpu::GpuDevice*)hwdevice_ctx)->device;

        device->AddRef();
        ctx->device = device;

        ctx->lock_ctx = (void *)1;
        ctx->lock     = DO_NOTHING;
        ctx->unlock   = DO_NOTHING;

        int err = av_hwdevice_ctx_init(ctx_buf);
        if(err) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, err));
            return NULL;
        }

        return ctx_buf;
    }


    Encoder*
    make_d3d11_encoder(int bitrate, char* codec)
    {
        static Encoder encoder = {0};
        encoder.name = "nvenc";
        encoder.profile = { 
            (int)h264::Profile::high, 
            (int)hevc::Profile::main, 
            (int)hevc::Profile::main_10 
        };


        /**
         * @brief 
         * make pixel format
         */
        #ifdef _WIN32
        encoder.dev_type = AV_HWDEVICE_TYPE_D3D11VA;
        encoder.dev_pix_fmt =  AV_PIX_FMT_D3D11;
        #else
        encoder.dev_type = AV_HWDEVICE_TYPE_CUDA;
        encoder.dev_pix_fmt = AV_PIX_FMT_CUDA;
        #endif
        encoder.static_pix_fmt = AV_PIX_FMT_NV12; 
        encoder.dynamic_pix_fmt = AV_PIX_FMT_P010;


        /**
         * @brief 
         * make device and encoding options
         */
        encoder.make_hw_ctx_func = dxgi_make_hwdevice_ctx;

        encoder.conf.bitrate = bitrate;
        encoder.conf.slicesPerFrame = 60;
        encoder.conf.enableDynamicRange = FALSE;
        encoder.conf.videoFormat = string_compare(codec,"h264") ? VideoFormat::H264 : VideoFormat::UNKNOWN;
        encoder.conf.videoFormat = string_compare(codec,"h265") ? VideoFormat::H265 : VideoFormat::UNKNOWN;
        encoder.codec_config =     string_compare(codec,"h264") ? h264::get_codec_config() : encoder.codec_config;
        encoder.codec_config =     string_compare(codec,"h265") ? hevc::get_codec_config() : encoder.codec_config;
        bool pass = encoder::validate_encoder(&encoder);
        if(pass)
            return &encoder;
        else 
            return NULL;
    }
}