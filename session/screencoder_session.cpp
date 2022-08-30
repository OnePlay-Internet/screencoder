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

#include <encoder_d3d11_device.h>
#include <display_base.h>

#include <thread>

using namespace std::literals;

namespace session {
    typedef struct _Session
    {
        util::Broadcaster* shutdown_event;

        util::QueueArray* packet_queue;

       sink::GenericSink* sink; 

       encoder::Encoder* encoder;

        platf::Display* display;
    }Session;




    void
    start_session(platf::Display* disp,
                  encoder::Encoder* encoder,
                  util::Broadcaster* shutdown,
                  sink::GenericSink* sink)
    {
        Session session;
        session.encoder = encoder;
        session.display = disp;
        session.shutdown_event = shutdown;
        session.sink = sink;
        session.packet_queue = QUEUE_ARRAY_CLASS->init();

        std::thread capture   { encoder::capture, 
                                session.display,
                                session.encoder,
                                session.sink,
                                session.shutdown_event, 
                                session.packet_queue };

        std::thread broadcast { sink::start_broadcast , 
                                session.sink,
                                session.shutdown_event, 
                                session.packet_queue };

        WAIT_EVENT(session.shutdown_event);
    }
}
