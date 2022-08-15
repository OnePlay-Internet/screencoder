/**
 * @file session.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include <screencoder_session.h>
#include <screencoder_util.h>
#include <screencoder_rtp.h>
#include <encoder_thread.h>

#include <thread>

namespace session {



    void        
    init_session(Session* session)
    {
        session->shutdown_event = NEW_EVENT;
        session->packet_queue = QUEUE_ARRAY_CLASS->init();
    }



    void
    start_session(Session* session)
    {
        std::thread capture   { encoder::capture, 
                                session->shutdown_event, 
                                session->packet_queue };

        std::thread broadcast { sink::start_broadcast , 
                                session->shutdown_event, 
                                session->packet_queue };

        WAIT_EVENT(session->shutdown_event);
        
    }
}
