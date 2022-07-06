/**
 * @file sunshine_bitstream.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_BITSTREAM_H__
#define __SUNSHINE_BITSTREAM_H__
extern "C" {
#include <cbs/cbs_h264.h>
#include <cbs/cbs_h265.h>
#include <cbs/video_levels.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

#include <sunshine_datatype.h>
#include <avcodec_datatype.h>

namespace bitstream
{
    typedef struct _Buffer {
        uint size;
        pointer ptr;
    }Buffer;

    typedef struct _NAL {
        Buffer _new;
        Buffer old;
    }NAL;

    typedef struct _HEVC {
        NAL sps;
        NAL vps;
    }HEVC;

    typedef struct _H264 {
        NAL sps;
    }H264;

    HEVC make_sps_hevc(const libav::CodecContext *ctx, const libav::Packet *packet);
    H264 make_sps_h264(const libav::CodecContext *ctx, const libav::Packet *packet);

    /**
     * Check if SPS->VUI is present
     */
    bool validate_sps(const libav::Packet *packet, int codec_id); 
} // namespace bitstream


#endif