/**
 * @file encoder_device.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_ENCODER_DEVICE_H__
#define __SUNSHINE_ENCODER_DEVICE_H__

#include <screencoder_util.h>
#include <platform_common.h>

#include <encoder_datatype.h>

namespace encoder
{
    typedef struct _Profile{
        int h264_high;
        int hevc_main;
        int hevc_main_10;
    } Profile;


    typedef enum _EncodingFlags{
        DEFAULT,
        PARALLEL_ENCODING,
        H264_ONLY,              // When HEVC is to heavy
        LIMITED_GOP_SIZE,       // Some encoders don't like it when you have an infinite GOP_SIZE. *cough* VAAPI *cough*
        SINGLE_SLICE_ONLY,      // Never use multiple slices <-- Older intel iGPU's ruin it for everyone else :P
        MAX_FLAGS_ENCODING
    }EncodingFlags;

    typedef enum _FrameFlags{
        PASSED,                // Is supported
        REF_FRAMES_RESTRICT,   // Set maximum reference frames
        REF_FRAMES_AUTOSELECT, // Allow encoder to select maximum reference frames (If !REF_FRAMES_RESTRICT --> REF_FRAMES_AUTOSELECT)
        SLICE,                 // Allow frame to be partitioned into multiple slices
        CBR,                   // Some encoders don't support CBR, if not supported --> attempt constant quantatication parameter instead
        DYNAMIC_RANGE,         // hdr
        MAX_FLAGS_FRAME
    }FrameFlags;


    typedef struct _CodecConfig{
        char* name;

        /**
         * @brief 
         * NULL terminated
         */
        util::KeyValue* qp;
        /**
         * @brief 
         * NULL terminated
         */
        util::KeyValue* options;

        std::bitset<FrameFlags::MAX_FLAGS_FRAME> capabilities;
    }CodecConfig;

    struct _Encoder{
        char* name;
        encoder::Config conf;

        CodecConfig h264;
        CodecConfig hevc;

        Profile profile;

        std::bitset<EncodingFlags::MAX_FLAGS_ENCODING> flags;

        libav::HWDeviceType dev_type;
        libav::PixelFormat dev_pix_fmt;

        libav::PixelFormat static_pix_fmt;
        libav::PixelFormat dynamic_pix_fmt;

        libav::BufferRef* (*make_hw_ctx_func) (platf::Device *hwdevice);
    };


    /**
     * @brief 
     * 
     * @param encoder 
     * @return true 
     * @return false 
     */
    bool        validate_encoder        (Encoder* encoder);
}

#endif