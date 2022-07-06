/**
 * @file sunshine_event.h
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

#include <sunshine_queue.h>

#define NEW_EVENT       event::new_event()
#define RAISE_EVENT(x)  event::raise_event(x)
#define WAIT_EVENT(x)   event::wait_event(x)

namespace event
{
    typedef util::QueueArray       Broadcaster;

    Broadcaster*    new_event       ();

    void            raise_event     (Broadcaster* broadcaster);

    bool            wait_event      (Broadcaster* broadcaster);
} // namespace event


#endif