/**
 * @file sunshine_config.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_config.h>


namespace config
{
    Encoder*       
    get_encoder_config()
    {
        // TODO
        static Encoder encoder;
        return &encoder;
    }

}
