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


#include <sunshine_session.h>
#include <sunshine_util.h>
#include <sunshine_rtp.h>
#include <encoder_thread.h>



namespace session {



    void        
    init_session(Session* session,
                 config::Encoder* config)
    {
        session->config = config;
        session->shutdown_event = NEW_EVENT;
        session->packet_queue = QUEUE_ARRAY_CLASS->init();
    }



    void
    start_session(Session* session)
    {
        // TODO
        encoder::capture(session->shutdown_event,
                         session->packet_queue,
                         NULL);

        rtp::start_broadcast(session->shutdown_event,
                             session->packet_queue);


        WAIT_EVENT(session->shutdown_event);
    }
}
