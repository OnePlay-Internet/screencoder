/**
 * @file screencoder_config.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_config.h>
#include <screencoder_util.h>

namespace config
{
    namespace nvenc {
        enum coder_e : int {
            _auto = 0,
            cabac,
            cavlc
        };
        enum rc_e {
            constqp   = 0x0,  /**< Constant QP mode */
            vbr       = 0x1,  /**< Variable bitrate mode */
            cbr       = 0x2,  /**< Constant bitrate mode */
            cbr_ld_hq = 0x8,  /**< low-delay CBR, high quality */
            cbr_hq    = 0x10, /**< CBR, high quality (slower) */
            vbr_hq    = 0x20,  /**< VBR, high quality (slower) */
            none
        };

        enum preset_e {
            _default = 0,
            slow,
            medium,
            fast,
            hp,
            hq,
            bd,
            ll_default,
            llhq,
            llhp,
            lossless_default, // lossless presets must be the last ones
            lossless_hp,
        };
    }

    namespace amd {
        enum class quality_e : int{
            _default = 0,
            speed,
            balanced,
        };

        enum class rc_hevc_e : int {
            constqp,     /**< Constant QP mode */
            vbr_latency, /**< Latency Constrained Variable Bitrate */
            vbr_peak,    /**< Peak Contrained Variable Bitrate */
            cbr,         /**< Constant bitrate mode */
        };

        enum class rc_h264_e : int {
            constqp,     /**< Constant QP mode */
            cbr,         /**< Constant bitrate mode */
            vbr_peak,    /**< Peak Contrained Variable Bitrate */
            vbr_latency, /**< Latency Constrained Variable Bitrate */
        };
    }

    Constant*       
    get_encoder_config()
    {
        static Constant encoder;
        RETURN_PTR_ONCE(encoder);
            
        encoder.gop_size = 1;
        encoder.gop_size_min = 0;
        encoder.ref_frame_num = 1;

        encoder.dwmflush = true;

        encoder.nv.rc = nvenc::rc_e::cbr;
        encoder.nv.coder = nvenc::coder_e::_auto;
        encoder.nv.preset = nvenc::preset_e::_default;

        encoder.amd.quality = (int)amd::quality_e::balanced; 
        encoder.amd.rc_h264 = (int)amd::rc_h264_e::cbr;
        encoder.amd.rc_hevc = (int)amd::rc_hevc_e::cbr;
        encoder.amd.coder = -1;
        return &encoder;
    }
}
