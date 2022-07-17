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

#define ENCODER_CONFIG          config::get_encoder_config(0,nullptr)




namespace config
{
    typedef struct NVidia {
        int preset;
        int rc;
        int coder;

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

        NVidia nv;

        char* encoder;
        char* adapter_name;
        char* output_name;

        int framerate;
        bool dwmflush;
    }Encoder;

    Encoder*       get_encoder_config       (int argc, char** argv);
}



#endif