/**
 * @file sunshine_config.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_config.h>
#include <sunshine_util.h>

namespace config
{
    enum coder_e : int {
        _auto = 0,
        cabac,
        cavlc
    };

    int 
    coder_from_view(char* coder) 
    {
        if(string_compare(coder,"auto")) 
            return coder_e::_auto;
        if(string_compare("cabac",coder) || 
           string_compare(coder,"ac")) 
            return coder_e::cabac;
        if(string_compare(coder,"cavlc") || 
           string_compare(coder,"vlc")) 
            return coder_e::cavlc;
        return -1;
    }

    enum rc_e {
        constqp   = 0x0,  /**< Constant QP mode */
        vbr       = 0x1,  /**< Variable bitrate mode */
        cbr       = 0x2,  /**< Constant bitrate mode */
        cbr_ld_hq = 0x8,  /**< low-delay CBR, high quality */
        cbr_hq    = 0x10, /**< CBR, high quality (slower) */
        vbr_hq    = 0x20,  /**< VBR, high quality (slower) */
        none
    };

    rc_e 
    rc_from_view(char* rc) {
        if(string_compare(rc,"constqp"))
            return rc_e::constqp;
        if(string_compare(rc,"vbr"))
            return rc_e::vbr;
        if(string_compare(rc,"cbr"))
            return rc_e::cbr;
        if(string_compare(rc,"cbr_hq"))
            return rc_e::cbr_hq;
        if(string_compare(rc,"vbr_hq"))
            return rc_e::vbr_hq;
        if(string_compare(rc,"cbr_ld_hq"))
            return rc_e::cbr_ld_hq;
        return rc_e::none;
    }

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


    Encoder*       
    get_encoder_config(int argc,char** argv)
    {
        static bool init = false;
        static Encoder encoder = {0};
        if (!argc && !argv && init)
            return &encoder;
            
        encoder.gop_size = 20;
        encoder.packet_size = RTSP_TCP_MAX_PACKET_SIZE;
        encoder.framerate = 60;
        encoder.dwmflush = 0;

        encoder.qp = 28;

        encoder.rtp.port = 6000;

        encoder.nv.coder = coder_e::_auto;
        encoder.nv.rc = rc_e::cbr;
        encoder.nv.rc = preset_e::_default;
        
        encoder.conf.width = 1920;
        encoder.conf.height = 1080;
        encoder.conf.bitrate = 10000;
        encoder.conf.framerate = 60;
        encoder.conf.encoderCscMode = 0;
        encoder.conf.videoFormat = 0;
        encoder.conf.dynamicRange = 0;
        encoder.conf.numRefFrames = 0;
        encoder.conf.slicesPerFrame = 1;

        init = true;
        return &encoder;
    }

    
    int 
    parse(int argc, 
          char** argv) 
    {
    }
}
