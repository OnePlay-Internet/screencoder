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
    set_bitrate(Session* session,
                SetBitrate func)
    {
        while (TRUE) {
            if (IS_INVOKED(session->shutdown_event))
                return;

            
            std::this_thread::sleep_for(5s);
            session->encoder->conf.bitrate = func();
        }
    }


    void
    start_session(SetBitrate bitrate_ctrl, 
                  SelectMonitor select,
                  sink::GenericSink* sink)
    {
        encoder::Encoder* encoder = NVENC(bitrate_ctrl(),"h265");
        platf::Display** displays = display::get_all_display(encoder);

        int i =0;
        platf::Display* display;
        while (*(displays+i)) {
            if (select((*(displays+i))->name)) {
                display = *(displays+i);
                goto start;
            }
            i++;
        }
        LOG_INFO("no match display");
        return; 
    start:
        Session session;
        session.encoder = encoder;
        session.display = display;
        session.shutdown_event = NEW_EVENT;
        session.packet_queue = QUEUE_ARRAY_CLASS->init();
        session.sink = sink;

        std::thread bitrate   { set_bitrate, 
                                &session,
                                bitrate_ctrl};

        std::thread capture   { encoder::capture, 
                                display,
                                encoder,
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
