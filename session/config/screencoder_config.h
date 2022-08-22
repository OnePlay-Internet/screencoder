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

#define SCREENCODER_CONSTANT          config::get_encoder_config()



#include <encoder_datatype.h>


namespace config
{
    typedef struct _NVconfig{
        int preset;
        int rc;
        int coder;
    }NVconfig;

    typedef struct _SWconfig {
        /**
         * @brief 
         * For software encoder
         */
        int min_threads; // Minimum number of threads/slices for CPU encoding
    }SWconfig;

    typedef struct _Constant{
        NVconfig nv;
        SWconfig sw;
        
        int qp; // higher == more compression and less quality
        int gop_size;
        int gop_size_min;
        int ref_frame_num;
        bool dwmflush;
    }Constant;

    Constant*       get_encoder_config       ();
}



#endif