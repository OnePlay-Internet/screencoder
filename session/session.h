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


#include <sunshine_util.h>
#include <sunshine_config.h>


namespace session
{
    typedef struct _Capturer
    {
        config::Encoder* config;

        util::Broadcaster* shutdown_event;
        util::Broadcaster* idr_event;

        util::QueueArray* packet_queue;

        util::QueueArray* capture_context_queue;
        util::QueueArray* capture_context_queue;

    }Capturer;
    
    typedef struct _Session Session;
    
} // namespace singleton


#endif