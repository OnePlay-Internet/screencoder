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


    Constant*       
    get_encoder_config()
    {
        static Constant encoder;
        RETURN_PTR_ONCE(encoder);
            
        encoder.qp = 28;
        encoder.gop_size = 10;
        encoder.gop_size_min = 3;
        encoder.dwmflush = true;
        encoder.nv.rc = rc_e::cbr;
        encoder.nv.coder = coder_e::_auto;
        encoder.nv.preset = preset_e::_default;
        return &encoder;
    }
}
