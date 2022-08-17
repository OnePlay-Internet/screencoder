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

#define ENCODER_CONFIG          config::get_encoder_config()



#include <encoder_datatype.h>


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
        Nvidia nv;
        encoder::Config conf;
        
        char* adapter_name;
        char* display_name;

        int qp; // higher == more compression and less quality
        int gop_size;
        int framerate;
        bool dwmflush;
    }Encoder;

    Encoder*       get_encoder_config       ();
}



#endif