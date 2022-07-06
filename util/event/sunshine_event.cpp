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


namespace event
{
    Broadcaster*    
    new_event()
    {
        return QUEUE_ARRAY_CLASS->init();
    }

    void            
    raise_event(Broadcaster* broadcaster)
    {
        object::Object* obj = OBJECT_CLASS->init((pointer)true,DO_NOTHING);
        QUEUE_ARRAY_CLASS->push(broadcaster,obj);
    }

    bool            
    wait_event(Broadcaster* broadcaster)
    {
        return QUEUE_ARRAY_CLASS->peek(broadcaster);
    }
} // namespace event