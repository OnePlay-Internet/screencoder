/**
 * @file sunshine_event.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_event.h>
#include <sunshine_macro.h>
#include <thread>

using namespace std::literals;

namespace util
{
    Broadcaster*    
    new_event()
    {
        return QUEUE_ARRAY_CLASS->init();
    }

    void            
    raise_event(Broadcaster* broadcaster)
    {
        Buffer* obj = BUFFER_CLASS->init((pointer)true,sizeof(bool),DO_NOTHING);
        QUEUE_ARRAY_CLASS->push(broadcaster,obj);
    }

    bool            
    wait_event(Broadcaster* broadcaster)
    {
        while (!QUEUE_ARRAY_CLASS->peek(broadcaster)) { std::this_thread::sleep_for(100ms); }
        return true;
    }

    bool            
    is_invoked(Broadcaster* broadcaster)
    {
        QUEUE_ARRAY_CLASS->peek(broadcaster);
        return true;
    }
} // namespace event