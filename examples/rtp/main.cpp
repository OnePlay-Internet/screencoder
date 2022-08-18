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
#include <typeinfo>
#include <iostream>

#include <screencoder_session.h>
#include <screencoder_config.h>
#include <encoder_d3d11_device.h>

#include <display_base.h>
#include <encoder_device.h>
#include <screencoder_rtp.h>
#include <iostream>


bool 
select_monitor (char* name)
{
    return TRUE;
}

int
set_bitrate ()
{
    return 1000;
}

int 
main(int argc, char ** argv)
{
    session::start_session(set_bitrate,select_monitor,RTP_SINK);
}