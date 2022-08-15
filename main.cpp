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
#include <sunshine_util.h>
#include <typeinfo>
#include <iostream>

#include <sunshine_session.h>
#include <sunshine_config.h>
#include <encoder_device.h>


#include <iostream>

int 
main(int argc, char ** argv)
{
    config::get_encoder_config(argc,argv);

    session::Session sess;
    memset(&sess,0,sizeof(session::Session));

    session::init_session(&sess);
    session::start_session(&sess);
    return 0;
}
