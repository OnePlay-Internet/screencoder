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
    int result_nvidia = QueryEncoder("nvenc_h264");
    int result_amd    = QueryEncoder("nvenc_h264");

    if (result_nvidia) 
        encoder = NVENC("h264");
    else if (result_amd)
        encoder = AMD("h264");

    if (!result_amd && !result_nvidia) {
        LOG_ERROR("no supported encoder");
        return 0;
    }
    


   

    platf::Display* display = platf::get_display_by_name(&encoder,NULL);
    util::Broadcaster* shutdown = NEW_EVENT;
    // std::thread wait10s {wait_shutdown,shutdown};
    // wait10s.detach();
    
    session::start_session(display,&encoder,shutdown,RTP_SINK);
    return 0;
}