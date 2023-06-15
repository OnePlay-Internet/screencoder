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
#include <screencoder_adaptive.h>
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

        encoder::Config config;

        platf::Display* display;

        util::QueueArray* capture_event_in;
        util::QueueArray* capture_event_out;
        util::QueueArray* sink_event_in;
        util::QueueArray* sink_event_out;
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
        session.packet_queue = QUEUE_ARRAY_CLASS->init(PACKET_QUEUE_SIZE);
        session.config = {
            encoder::SlicePerFrame::TWO,
            encoder::DynamicRange::DISABLE,
            (AVColorRange)encoder::LibavColor::JPEG,
            encoder::LibscaleColor::REC_601
        };

        session.capture_event_in = QUEUE_ARRAY_CLASS->init(INFINITE_);
        session.capture_event_out = QUEUE_ARRAY_CLASS->init(INFINITE_);
        session.sink_event_in = sink->get_input_eve(sink);
        session.sink_event_out = sink->get_output_eve(sink);



        std::thread capture   { encoder::capture, 
                                session.display,
                                session.encoder,
                                &session.config,
                                session.sink,
                                session.capture_event_in,
                                session.capture_event_out,
                                session.shutdown_event, 
                                session.packet_queue };

        std::thread broadcast { sink::start_broadcast , 
                                session.sink,
                                session.shutdown_event, 
                                session.packet_queue };

        std::thread control   { adaptive::newAdaptiveControl, 
                                session.shutdown_event, 
                                session.packet_queue,
                                session.capture_event_in,
                                session.capture_event_out,
                                session.sink_event_in,
                                session.sink_event_out};

        capture.detach();
        broadcast.detach();
        control.detach();
        WAIT_EVENT(session.shutdown_event);
    }
}
