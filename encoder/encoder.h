/**
 * @file encoder.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __ENCODER_H__
#define __ENCODER_H__

#include <sunshine_util.h>

#include <encoder_validate.h>
#include <common.h>



namespace encoder
{
    typedef struct _Session {
        libav::CodecContext* context;
        platf::HWDevice* device;

        int inject;
    }Session;

    typedef struct _SessionClass{
        
    } SessionClass;

    typedef struct _Profile{
        int h264_high;
        int hevc_main;
        int hevc_main_10;
    } Profile;

    typedef libav::BufferRef* (*MakeHWDeviceContext) (platf::HWDevice *hwdevice);

    typedef enum _EncodingFlags{
        DEFAULT           = 0x00,
        PARALLEL_ENCODING = 0x01,
        H264_ONLY         = 0x02, // When HEVC is to heavy
        LIMITED_GOP_SIZE  = 0x04, // Some encoders don't like it when you have an infinite GOP_SIZE. *cough* VAAPI *cough*
        SINGLE_SLICE_ONLY = 0x08, // Never use multiple slices <-- Older intel iGPU's ruin it for everyone else :P
    }EncodingFlags;

    typedef enum _FrameFlags{
        PASSED,                // Is supported
        REF_FRAMES_RESTRICT,   // Set maximum reference frames
        REF_FRAMES_AUTOSELECT, // Allow encoder to select maximum reference frames (If !REF_FRAMES_RESTRICT --> REF_FRAMES_AUTOSELECT)
        SLICE,                 // Allow frame to be partitioned into multiple slices
        CBR,                   // Some encoders don't support CBR, if not supported --> attempt constant quantatication parameter instead
        DYNAMIC_RANGE,         // hdr
        VUI_PARAMETERS,        // AMD encoder with VAAPI doesn't add VUI parameters to SPS
        NALU_PREFIX_5b,        // libx264/libx265 have a 3-byte nalu prefix instead of 4-byte nalu prefix
        MAX_FLAGS
    }FrameFlags;

    typedef enum _ValidateFlags{
        VUI_PARAMS     = 0x01,
        NALU_PREFIX_5B = 0x02,
    }ValidateFlags;

    typedef enum _Type
    {
        STRING,
        INT,
    }Type;

    typedef struct _KeyValue {
        char* key;
        Type type;
        char* string_value;
        int int_value;
    }KeyValue;

    typedef struct _CodecConfig{
        char* name;
        bool has_qp;

        /**
         * @brief 
         * NULL terminated
         */
        KeyValue* qp;
        /**
         * @brief 
         * NULL terminated
         */
        KeyValue* options;
        std::bitset<MAX_FLAGS> capabilities;
    }CodecConfig;

    typedef struct _Encoder{
        char* name;

        CodecConfig h264;
        CodecConfig hevc;

        Profile profile;

        libav::HWDeviceType dev_type;
        libav::PixelFormat dev_pix_fmt;

        libav::PixelFormat static_pix_fmt;
        libav::PixelFormat dynamic_pix_fmt;

        int flags;

        MakeHWDeviceContext make_hw_ctx_func;
    }Encoder;


    platf::PixelFormat  map_pix_fmt     (libav::PixelFormat fmt);


    int 
    validate_config(platf::Display* disp, 
                    Encoder* encoder, 
                    Config* config) ;
} // namespace error


#endif