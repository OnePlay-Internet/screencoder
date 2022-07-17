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
#include <encoder_packet.h>
#include <typeinfo>
#include <iostream>


int 
main(int argc, char const *argv[])
{
    encoder::Config* conf;
    std::cout << typeid((encoder::Packet*)conf).name() << std::endl;
    return 0;
}
