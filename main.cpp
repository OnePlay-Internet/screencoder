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
#include <typeinfo>
#include <iostream>
#include <sunshine_session.h>


int 
main(int argc, char const *argv[])
{
    session::Session sess;
    memset(&sess,0,sizeof(session::Session));

    session::init_session(&sess,NULL);
    session::start_session(&sess);
    return 0;
}
