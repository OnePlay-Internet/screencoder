/**
 * @file capturer.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __SUNSHINE_CAPTURER_H__
#define __SUNSHINE_CAPTURER_H__


#include <screencoder_util.h>
#include <screencoder_config.h>
#include <generic_sink.h>


namespace session
{
    
    typedef bool (*SelectMonitor)   (char* name);

    typedef int (*SetBitrate)       ();

    void        start_session       (SetBitrate bitrate_control,
                                     SelectMonitor select,
                                     sink::GenericSink* sink);
} // namespace singleton


#endif