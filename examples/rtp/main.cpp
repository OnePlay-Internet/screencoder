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

using namespace std::literals;

bool 
select_monitor (char* name)
{
    return TRUE;
}

void 
wait_shutdown(util::Broadcaster* event)
{
    std::this_thread::sleep_for(10s);
    RAISE_EVENT(event);
}

int 
main(int argc, char ** argv)
{
    encoder::Encoder* encoder = NVENC("h265");
    if(!encoder) {
        LOG_ERROR("NVENC encoder is not ready");
        return 0;
    }

    platf::Display** displays = display::get_all_display(encoder);

    int i =0;
    platf::Display* display;
    while (*(displays+i)) {
        if (select_monitor((*(displays+i))->name)) {
            display = *(displays+i);
            goto start;
        }
        i++;
    }
    LOG_INFO("no match display");
    return 0; 
start:
    util::Broadcaster* shutdown = NEW_EVENT;
    std::thread {wait_shutdown,shutdown};
    session::start_session(display,encoder,shutdown,RTP_SINK);
    return 0;
}