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

namespace nvcodec{
    namespace h264 {
        typedef enum _Profile{
            baseline,
            main,
            high,
            high_444p,
        }Profile;

        encoder::CodecConfig*
        get_codec_config()
        {
            static encoder::CodecConfig ret;
            RETURN_PTR_ONCE(ret);
            ret.format = encoder::VideoFormat::H264;
            util::KeyValue* h264pairs = util::new_keyvalue_pairs(6);
            util::keyval_new_intval(h264pairs,"zerolatency",1);
            util::keyval_new_intval(h264pairs,"preset",SCREENCODER_CONSTANT->nv.rc);
            util::keyval_new_intval(h264pairs,"rc",SCREENCODER_CONSTANT->nv.rc);
            util::keyval_new_intval(h264pairs,"coder",SCREENCODER_CONSTANT->nv.coder);
            ret.name = "h264_nvenc";
            ret.options = h264pairs;
            return &ret;
        }
    } // namespace nv

    namespace hevc {
        typedef enum _Profile{
            main,
            main_10,
            rext,
        }Profile;

        encoder::CodecConfig*
        get_codec_config()
        {        
            static encoder::CodecConfig ret;
            RETURN_PTR_ONCE(ret);
            ret.format = encoder::VideoFormat::H265;
            util::KeyValue* hevcpairs = util::new_keyvalue_pairs(5);
            util::keyval_new_intval(hevcpairs,"zerolatency",1);
            util::keyval_new_intval(hevcpairs,"preset",SCREENCODER_CONSTANT->nv.rc);
            util::keyval_new_intval(hevcpairs,"rc",SCREENCODER_CONSTANT->nv.rc);
            ret.name = "hevc_nvenc";
            ret.options = hevcpairs;
            return &ret;
        }
    }
}

namespace amd{
    namespace h264 {
        typedef enum _Profile{
            baseline,
            main,
            high,
            high_444p,
        }Profile;

        encoder::CodecConfig*
        get_codec_config()
        {
            static encoder::CodecConfig ret;
            RETURN_PTR_ONCE(ret);
            ret.format = encoder::VideoFormat::H264;
            util::KeyValue* h264pairs = util::new_keyvalue_pairs(6);

            util::keyval_new_strval(h264pairs,"usage", "ultralowlatency" );
            util::keyval_new_strval(h264pairs,"log_to_dbg", "1" );
            util::keyval_new_intval(h264pairs,"quality", SCREENCODER_CONSTANT->amd.quality );
            util::keyval_new_intval(h264pairs,"rc", SCREENCODER_CONSTANT->amd.rc_h264 );
            ret.name = "h264_amf";
            ret.options = h264pairs;
            return &ret;
        }
    } // namespace nv

    namespace hevc {
        typedef enum _Profile{
            main,
            main_10,
            rext,
        }Profile;

        encoder::CodecConfig*
        get_codec_config()
        {        
            static encoder::CodecConfig ret;
            RETURN_PTR_ONCE(ret);

            ret.name = "hevc_amf";
            ret.format = encoder::VideoFormat::H265;
            util::KeyValue* hevcpairs = util::new_keyvalue_pairs(5);
            util::keyval_new_strval(hevcpairs,"header_insertion_mode", "idr");
            util::keyval_new_intval(hevcpairs,"gops_per_idr", 30);
            util::keyval_new_strval(hevcpairs,"usage", "ultralowlatency");
            util::keyval_new_intval(hevcpairs,"quality", SCREENCODER_CONSTANT->amd.quality);
            util::keyval_new_intval(hevcpairs,"rc", SCREENCODER_CONSTANT->amd.rc_hevc);
            ret.options = hevcpairs;
            return &ret;
        }
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



    Encoder
    make_d3d11_nvidia_encoder(char* codec)
    {
        Encoder encoder = {0};
        encoder.name = "nvenc";
        encoder.profile = { 
            (int)nvcodec::h264::Profile::high, 
            (int)nvcodec::hevc::Profile::main, 
            (int)nvcodec::hevc::Profile::main_10 
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
        encoder.codec_config =     string_compare(codec,"h264") ? nvcodec::h264::get_codec_config() : encoder.codec_config;
        encoder.codec_config =     string_compare(codec,"h265") ? nvcodec::hevc::get_codec_config() : encoder.codec_config;

        Capabilities pass = encoder::validate_encoder(&encoder);
        return encoder;
    }

    Encoder
    make_d3d11_amd_encoder(char* codec)
    {
        Encoder encoder = {0};
        encoder.name = "amdvce";
        encoder.profile = { 
            FF_PROFILE_H264_HIGH,
            FF_PROFILE_HEVC_MAIN
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
        encoder.codec_config =     string_compare(codec,"h264") ? amd::h264::get_codec_config() : encoder.codec_config;
        encoder.codec_config =     string_compare(codec,"h265") ? amd::hevc::get_codec_config() : encoder.codec_config;

        Capabilities pass = encoder::validate_encoder(&encoder);
        return encoder;
    }
}