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

#include __SUNSHINE_CAPTURER_H__
#define __SUNSHINE_CAPTURER_H__


#include <sunshine_queue.h>


namespace singleton
{
    typedef struct _Capturer
    {
        util::QueueArray* shutdown_event;
        util::QueueArray* packet_queue;
        util::QueueArray* idr_event;

        util::QueueArray* capture_context_queue;
        util::QueueArray* capture_context_queue;

    }Capturer;
    
    
} // namespace singleton


#endif