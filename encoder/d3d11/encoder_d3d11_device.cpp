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

#include <encoder.h>
#include <avcodec_datatype.h>
#include <encoder.h>
#include <common.h>

#include <sunshine_config.h>

#ifdef _WIN32
extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}
#endif


namespace nv {
    typedef enum _ProfileH264{
        baseline,
        main,
        high,
        high_444p,
    }ProfileH264;

    typedef enum _ProfileHEVC{
        main,
        main_10,
        rext,
    }ProfileHEVC;
} // namespace nv

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

    int init() {
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////////"sv;
        // BOOST_LOG(info) << "//                                                              //"sv;
        // BOOST_LOG(info) << "//   Testing for available encoders, this may generate errors.  //"sv;
        // BOOST_LOG(info) << "//   You can safely ignore those errors.                        //"sv;
        // BOOST_LOG(info) << "//                                                              //"sv;
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////////"sv;

        KITTY_WHILE_LOOP(auto pos = std::begin(encoders), pos != std::end(encoders), {
            if(
            (!config::video.encoder.empty() && pos->name != config::video.encoder) ||
            !validate_encoder(*pos) ||
            (config::video.hevc_mode == 3 && !pos->hevc[encoder_t::DYNAMIC_RANGE])) {
            pos = encoders.erase(pos);

            continue;
            }

            break;
        })

        // BOOST_LOG(info);
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////"sv;
        // BOOST_LOG(info) << "//                                                          //"sv;
        // BOOST_LOG(info) << "// Ignore any errors mentioned above, they are not relevant //"sv;
        // BOOST_LOG(info) << "//                                                          //"sv;
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////"sv;
        // BOOST_LOG(info);

        if(encoders.empty()) {
            if(config::video.encoder.empty()) {
            BOOST_LOG(fatal) << "Couldn't find any encoder"sv;
            }
            else {
            BOOST_LOG(fatal) << "Couldn't find any encoder matching ["sv << config::video.encoder << ']';
            }

            return -1;
        }

        auto &encoder = encoders.front();

        BOOST_LOG(debug) << "------  h264 ------"sv;
        for(int x = 0; x < encoder_t::MAX_FLAGS; ++x) {
            auto flag = (encoder_t::flag_e)x;
            BOOST_LOG(debug) << encoder_t::from_flag(flag) << (encoder.h264[flag] ? ": supported"sv : ": unsupported"sv);
        }
        BOOST_LOG(debug) << "-------------------"sv;

        if(encoder.hevc[encoder_t::PASSED]) {
            BOOST_LOG(debug) << "------  hevc ------"sv;
            for(int x = 0; x < encoder_t::MAX_FLAGS; ++x) {
            auto flag = (encoder_t::flag_e)x;
            BOOST_LOG(debug) << encoder_t::from_flag(flag) << (encoder.hevc[flag] ? ": supported"sv : ": unsupported"sv);
            }
            BOOST_LOG(debug) << "-------------------"sv;

            BOOST_LOG(info) << "Found encoder "sv << encoder.name << ": ["sv << encoder.h264.name << ", "sv << encoder.hevc.name << ']';
        }
        else {
            BOOST_LOG(info) << "Found encoder "sv << encoder.name << ": ["sv << encoder.h264.name << ']';
        }

        if(config::video.hevc_mode == 0) {
            config::video.hevc_mode = encoder.hevc[encoder_t::PASSED] ? (encoder.hevc[encoder_t::DYNAMIC_RANGE] ? 3 : 2) : 1;
        }

        return 0;
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
            (int)nv::ProfileH264::high, 
            (int)nv::ProfileHEVC::main, 
            (int)nv::ProfileHEVC::main_10 
        };

        encoder.hevc = 
        {
            "nvenv_hevc",
            FALSE,
            0,
        }

        encoder.h264 = 
        {
            "nvenv_h264",
            TRUE,
            ENCODER_CONFIG->qp,
        }


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