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

        bool hardware;
        
        int sws_color_space;
    };
    
    struct _Config{
        int bitrate;

        int slicesPerFrame;
        int numRefFrames;

        int encoderCscMode;

        int videoFormat;
        int dynamicRange;
    };

} // namespace encoder


#endif