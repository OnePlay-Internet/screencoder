/**
 * @file encoder_datatype.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __ENCODER_DATATYPE_H__
#define __ENCODER_DATATYPE_H__

#include <encoder_d3d11_device.h>



namespace encoder
{
    typedef struct _Session Session;

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



    typedef struct _Encoder{
        char* name;


        Profile profile;

        AVHWDeviceType dev_type;
        AVPixelFormat dev_pix_fmt;

        AVPixelFormat static_pix_fmt;
        AVPixelFormat dynamic_pix_fmt;

        int flags;

        MakeHWDeviceContext make_hw_ctx_func;
    }Encoder;



} // namespace error


#endif