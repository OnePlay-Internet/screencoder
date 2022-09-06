/**
 * @file screencoder_event.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_EVENT_H__
#define __SUNSHINE_EVENT_H__

#include <screencoder_queue.h>

#define NEW_EVENT       util::new_event()
#define RAISE_EVENT(x)  util::raise_event(x)
#define DESTROY_EVENT(x)util::destroy_event(x)
#define WAIT_EVENT(x)   util::wait_event(x)
#define IS_INVOKED(x)   util::is_invoked(x)

namespace util
{
    typedef util::QueueArray       Broadcaster;

    Broadcaster*    new_event       ();

    void            destroy_event   (Broadcaster* broadcaster);

    void            raise_event     (Broadcaster* broadcaster);

    bool            wait_event      (Broadcaster* broadcaster);

    bool            is_invoked      (Broadcaster* broadcaster);
} // namespace event


#endif