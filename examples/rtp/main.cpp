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
#include <encoder_device.h>


#include <iostream>

int 
main(int argc, char ** argv)
{
    session::Session sess = {0};
    session::init_session(&sess);
    session::start_session(&sess);
    return 0;
}