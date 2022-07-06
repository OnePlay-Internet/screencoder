
#include <encoder_datatype.h>
#include <encoder_d3d11_device.h>
#include <avcodec_datatype.h>
#include <sunshine_log.h>
#include <sunshine_macro.h>
#include <encoder_datatype.h>
#include <common.h>

#ifdef _WIN32
extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}
#endif


namespace nv {
    enum class profile_h264_e : int {
    baseline,
    main,
    high,
    high_444p,
    };

    enum class profile_hevc_e : int {
    main,
    main_10,
    rext,
    };
} // namespace nv

namespace encoder {

    /**
     * @brief 
     * 
     * @param hwdevice_ctx 
     * @return libav::BufferRef* 
     */
    libav::BufferRef* 
    dxgi_make_hwdevice_ctx(platf::hwdevice_t *hwdevice_ctx) 
    {
        libav::BufferRef* ctx_buf { av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA) };
        auto ctx = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)ctx_buf->data)->hwctx;

        std::fill_n((std::uint8_t *)ctx, sizeof(AVD3D11VADeviceContext), 0);

        auto device = (ID3D11Device *)hwdevice_ctx->data;

        device->AddRef();
        ctx->device = device;

        ctx->lock_ctx = (void *)1;
        ctx->lock     = DO_NOTHING;
        ctx->unlock   = DO_NOTHING;

        auto err = av_hwdevice_ctx_init(ctx_buf);
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
            (int)nv::profile_h264_e::high, 
            (int)nv::profile_hevc_e::main, 
            (int)nv::profile_hevc_e::main_10 
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
        #ifdef _WIN32
        encoder.flags = DEFAULT;
        encoder.make_hw_ctx_func = dxgi_make_hwdevice_ctx;
        #else
        encoder.flags = PARALLEL_ENCODING;
        encoder.make_hw_ctx_func = cuda_make_hwdevice_ctx;
        #endif

        initialized = true;
        return &encoder;
    }
}