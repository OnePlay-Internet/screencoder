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
    /**
     * Check if SPS->VUI is present
     */
    bool        validate_packet         (const libav::Packet *packet, 
                                         int codec_id); 
} // namespace bitstream


#endif