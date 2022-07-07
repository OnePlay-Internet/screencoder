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

#ifndef __ENCODER_PACKET_H__
#define __ENCODER_PACKET_H__
#include <avcodec_datatype.h>
#include <sunshine_datatype.h>



using float4 = float[4];
using float3 = float[3];
using float2 = float[2];


namespace encoder
{
    typedef struct _Replace {
        pointer old;
        pointer _new;
    }Replace;

    typedef struct _Config{
        int width;
        int height;
        int framerate;
        int bitrate;
        int slicesPerFrame;
        int numRefFrames;
        int encoderCscMode;
        int videoFormat;
        int dynamicRange;
    }Config;

    typedef struct _Color {
        float4 color_vec_y;
        float4 color_vec_u;
        float4 color_vec_v;
        float2 range_y;
        float2 range_uv;
    }Color;

    typedef struct _Packet{
        libav::Packet* packet;
        
        Replace* replacements;

        pointer user_data;
    }Packet;

    typedef struct _PacketClass{
        Packet* (*init) ();



        void    (*finalize) (Packet* packet);
    }PacketClass;

    Color*          get_color           ();

    void            capture             ( pointer mail, 
                                          Config config, 
                                          pointer user_data);
} // namespace error


#endif