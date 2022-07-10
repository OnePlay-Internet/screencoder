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

namespace config
{
    typedef struct _Encoder{
        // ffmpeg params
        int qp; // higher == more compression and less quality

        int hevc_mode;

        int min_threads; // Minimum number of threads/slices for CPU encoding

        struct {
            char* preset;
            char* tune;
        } sw;

        struct {
            int preset;
            int rc;
            int coder;
        } nv;

        struct {
            int quality;
            int rc_h264;
            int rc_hevc;
            int coder;
        } amd;

        struct {
            int allow_sw;
            int require_sw;
            int realtime;
            int coder;
        } vt;

        char* encoder;
        char* adapter_name;
        char* output_name;

        int framerate;
        bool dwmflush;
    }Encoder;

    Encoder*       get_encoder_config       ();
}



#endif