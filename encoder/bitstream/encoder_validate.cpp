/**
 * @file sunshine_bitstream.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_validate.h>
#include <sunshine_util.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

#include <string.h>



namespace encoder {


    bool 
    validate_packet(libav::Packet* packet, 
                 int codec_id) 
    {
    }
} // namespace cbs