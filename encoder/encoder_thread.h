/**
 * @file encoder_thread.h
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

#include <platform_common.h>



namespace encoder
{
    typedef struct _Session {
        libav::CodecContext* context;
        libav::FormatContext* format_context;
        libav::Stream* stream;

        platf::HWDevice* device;

        int64 pts;
    }Session;


    typedef struct _Profile{
        int h264_high;
        int hevc_main;
        int hevc_main_10;
    } Profile;

    typedef libav::BufferRef* (*MakeHWDeviceContext) (platf::HWDevice *hwdevice);

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

    typedef struct _Encoder{
        char* name;

        CodecConfig h264;
        CodecConfig hevc;

        Profile profile;

        libav::HWDeviceType dev_type;
        libav::PixelFormat dev_pix_fmt;

        libav::PixelFormat static_pix_fmt;
        libav::PixelFormat dynamic_pix_fmt;

        std::bitset<EncodingFlags::MAX_FLAGS_ENCODING> flags;

        MakeHWDeviceContext make_hw_ctx_func;
    }Encoder;


    platf::PixelFormat  map_pix_fmt     (libav::PixelFormat fmt);


    /**
     * @brief 
     * 
     * @param disp 
     * @param encoder 
     * @param config 
     * @return int 
     */
    bool                validate_config (Encoder* encoder, 
                                         Config* config) ;    
    
    /**
     * @brief 
     * 
     * @param shutdown_event 
     * @param packet_queue 
     * @param config 
     * @param data 
     */
    void                capture          (util::Broadcaster* shutdown_event,
                                          util::QueueArray* packet_queue,
                                          Config* config,
                                          pointer data);
} // namespace error


#endif