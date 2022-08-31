/**
 * @file encoder_datatype.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __ENCODER_DATATYPE_H__
#define __ENCODER_DATATYPE_H__

#include <screencoder_util.h>
#include <platform_common.h>

namespace encoder
{    
    struct _Session {
        libav::CodecContext* context;

        libav::Codec* codec;

        platf::Device* device;

        int sws_color_space;

        int dev_type;
    };

    enum LibavColor{
        MPEG = AVCOL_RANGE_MPEG,
        JPEG = AVCOL_RANGE_JPEG,
        LibavColorMax,
    };
    enum LibscaleColor{
        REC_601,
        REC_709,
        REC_2020,
        LIBSCALE_COLOR_MAX,
    };

    enum VideoFormat {
        UNKNOWN,
        H264,
        H265,
        VIDEO_FORMAT_MAX,
    };

    enum SlicePerFrame{
        ONE = 1,
        TWO = 2,
        SLICE_PER_FRAME_MAX,
    };

    enum DynamicRange{
        DISABLE = 0,
        ENABLE = 1,
        DYNAMIC_RANGE_MAX,
    };

    struct _Config{
        int bitrate;

        SlicePerFrame slicesPerFrame;
        DynamicRange dynamicRangeOption;
        AVColorRange avcolor;
        LibscaleColor scalecolor;
    };

    typedef enum _FrameFlags{
        PASSED,                // Is supported
        SLICE,                 // Allow frame to be partitioned into multiple slices
        CBR,                   // Some encoders don't support CBR, if not supported --> attempt constant quantatication parameter instead
        DYNAMIC_RANGE,         // hdr
        MAX_FLAGS_FRAME
    }FrameFlags;

    typedef std::bitset<FrameFlags::MAX_FLAGS_FRAME> Capabilities;
} // namespace encoder


#endif