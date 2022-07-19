/**
 * @file config.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_CONFIG_H__
#define __SUNSHINE_CONFIG_H__
#include <encoder_thread.h>

#define ENCODER_CONFIG          config::get_encoder_config(0,nullptr)

#define RTSP_TCP_MAX_PACKET_SIZE 1472




namespace config
{
    typedef struct _NVidia {
        int preset;
        int rc;
        int coder;
    }Nvidia;

    typedef struct _RTP {
        int port;
    }RTP;

    typedef struct SW {
        /**
         * @brief 
         * For software encoder
         */
        int min_threads; // Minimum number of threads/slices for CPU encoding
    };

    typedef struct _Encoder{
        // ffmpeg params
        int qp; // higher == more compression and less quality

        int hevc_mode;

        Nvidia nv;
        RTP rtp;
        
        encoder::Config *encoder_config;

        char* encoder;
        char* adapter_name;
        char* output_name;


        int packet_size;
        int framerate;
        bool dwmflush;



    }Encoder;

    Encoder*       get_encoder_config       (int argc, char** argv);
}



#endif