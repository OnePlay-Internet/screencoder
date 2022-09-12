/**
 * @file main.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_util.h>

#include <screencoder_session.h>
#include <screencoder_config.h>
#include <encoder_d3d11_device.h>

#include <display_base.h>
#include <encoder_device.h>
#include <screencoder_rtp.h>
#include <iostream>

#include <thread>

extern "C" {
#include <go_adapter.h>
}

using namespace std::literals;


void 
wait_shutdown(util::Broadcaster* event)
{
    std::this_thread::sleep_for(10s);
    RAISE_EVENT(event);
}


#include <memory>
#include <iostream>
#include <functional>


int 
main(int argc, char ** argv)
{
    std::shared_ptr<void> _{ NULL,[](...) {
        LOG_INFO("waiting for log to flush");
        std::this_thread::sleep_for(5s);
    } };

    encoder::Encoder encoder;
    if (string_compare(*(argv+1),"nvidia")) 
        encoder = NVENC("h264");
    else if (string_compare(*(argv+1),"amd"))
        encoder = AMD("h264");
    else 
        return 0;
    


   
    
    char* display_name = QueryDisplay(0);

    if(!encoder.codec_config->capabilities[encoder::FrameFlags::PASSED]) {
        LOG_ERROR("NVENC encoder is not ready");
        return 0;
    }

    platf::Display** displays = display::get_all_display(&encoder);

    int i =0;
    platf::Display* display;
    while (*(displays+i)) {
        if (string_compare((*(displays+i))->name,display_name)) {
            display = *(displays+i);
            goto start;
        }
        i++;
    }
    LOG_INFO("no match display");
    return 0; 
start:
    util::Broadcaster* shutdown = NEW_EVENT;
    // std::thread wait10s {wait_shutdown,shutdown};
    // wait10s.detach();
    
    util::QueueArray* sink_in = QUEUE_ARRAY_CLASS->init();
    util::QueueArray* sink_out = QUEUE_ARRAY_CLASS->init();
    session::start_session(display,&encoder,
        sink_in,
        sink_out,
        shutdown,RTP_SINK(sink_in,sink_out));
    return 0;
}