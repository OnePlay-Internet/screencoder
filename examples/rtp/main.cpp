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

#include <iostream>

int 
main(int argc, char ** argv)
{
    char** display_names = platf::display_names(platf::MemoryType::system);

    int count = 0;
    while (*(display_names+count))
    {
        char* chosen_display = *(display_names+count);
        printf("%s\n",chosen_display);
        count++;
    }

    display::get_all_display(NVENC);
    return 0;
}
