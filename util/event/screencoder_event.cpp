/**
 * @file screencoder_event.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_event.h>
#include <screencoder_macro.h>
#include <thread>

using namespace std::literals;

namespace util
{
    Broadcaster*    
    new_event()
    {
        return QUEUE_ARRAY_CLASS->init(INFINITE_);
    }

    void
    destroy_event(Broadcaster* event) 
    {
        return QUEUE_ARRAY_CLASS->stop(event);
    }

    void            
    raise_event(Broadcaster* broadcaster)
    {
        Buffer* obj = BUFFER_INIT((pointer)true,sizeof(bool),DO_NOTHING);
        QUEUE_ARRAY_CLASS->push(broadcaster,obj,false);
        BUFFER_UNREF(obj);
    }

    bool            
    wait_event(Broadcaster* broadcaster)
    {
        while (!QUEUE_ARRAY_CLASS->peek(broadcaster)) { 
            std::this_thread::sleep_for(10s); 
        }
        return true;
    }

    bool            
    is_invoked(Broadcaster* broadcaster)
    {
        return QUEUE_ARRAY_CLASS->peek(broadcaster);
    }
} // namespace event