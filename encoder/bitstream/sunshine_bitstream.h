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
#include <sunshine_util.h>


namespace bitstream {
    typedef struct _NAL {
        util::Buffer* _new;
        util::Buffer* old;
    }NAL;

    typedef struct _HEVC {
        NAL sps;
        NAL vps;
    }HEVC;

    typedef struct _H264 {
        NAL sps;
    }H264;

    /**
     * @brief 
     * 
     * @param ctx 
     * @param packet 
     * @return HEVC 
     */
    HEVC        make_sps_hevc           (const libav::CodecContext *ctx, 
                                         const libav::Packet *packet);

    /**
     * @brief 
     * 
     * @param ctx 
     * @param packet 
     * @return H264 
     */
    H264        make_sps_h264           (const libav::CodecContext *ctx, 
                                         const libav::Packet *packet);

    /**
     * Check if SPS->VUI is present
     */
    bool        validate_sps            (const libav::Packet *packet, 
                                         int codec_id); 
} // namespace bitstream


#endif