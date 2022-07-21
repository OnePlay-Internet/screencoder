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
#include <sunshine_util.h>

#include <encoder_thread.h>
#include <encoder_device.h>
#include <platform_common.h>

#include <sunshine_config.h>

#ifdef _WIN32
extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}
#endif


namespace h264 {
    typedef enum _Profile{
        baseline,
        main,
        high,
        high_444p,
    }Profile;
} // namespace nv

namespace hevc {
    typedef enum _Profile{
        main,
        main_10,
        rext,
    }Profile;
}

namespace encoder {

    /**
     * @brief 
     * 
     * @param hwdevice_ctx 
     * @return libav::BufferRef* 
     */
    libav::BufferRef* 
    dxgi_make_hwdevice_ctx(platf::HWDevice *hwdevice_ctx) 
    {
        libav::BufferRef* ctx_buf = (libav::BufferRef*)av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        AVD3D11VADeviceContext* ctx = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)ctx_buf->data)->hwctx;
        memset((pointer) ctx, 0, sizeof (AVD3D11VADeviceContext));


        ID3D11Device *device = (ID3D11Device *)hwdevice_ctx->data;

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
    make_d3d11_encoder()
    {
        static bool initialized = false;
        static Encoder encoder = {0};
        if (initialized)
            return &encoder;
        
        encoder.name = "nvenc";
        encoder.profile = 
        { 
            (int)h264::Profile::high, 
            (int)hevc::Profile::main, 
            (int)hevc::Profile::main_10 
        };

        util::KeyValue* hevcqp = util::new_keyvalue_pairs(1);
        util::KeyValue* hevcpairs = util::new_keyvalue_pairs(5);
        util::keyval_new_intval(hevcpairs,"forced-idr",1);
        util::keyval_new_intval(hevcpairs,"zerolatency",1);
        util::keyval_new_intval(hevcpairs,"preset",ENCODER_CONFIG->nv.rc);
        util::keyval_new_intval(hevcpairs,"rc",ENCODER_CONFIG->nv.rc);
        encoder.hevc = CodecConfig {
            "nvenc_hevc",
            hevcqp,
            hevcpairs,
        };

        util::KeyValue* h264qp = util::new_keyvalue_pairs(2);
        util::keyval_new_intval(h264qp,"qp",ENCODER_CONFIG->qp);
        util::KeyValue* h264pairs = util::new_keyvalue_pairs(6);
        util::keyval_new_intval(h264pairs,"forced-idr",1);
        util::keyval_new_intval(h264pairs,"zerolatency",1);
        util::keyval_new_intval(h264pairs,"preset",ENCODER_CONFIG->nv.rc);
        util::keyval_new_intval(h264pairs,"rc",ENCODER_CONFIG->nv.rc);
        util::keyval_new_intval(h264pairs,"coder",ENCODER_CONFIG->nv.coder);
        encoder.h264 = 
        {
            "nvenc_h264",
            h264qp,
            h264pairs,
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
        encoder.flags.set().flip();
        encoder.flags[EncodingFlags::DEFAULT] = true;
        encoder.make_hw_ctx_func = dxgi_make_hwdevice_ctx;

        initialized = true;
        encoder::validate_encoder(&encoder);
        return &encoder;
    }
}