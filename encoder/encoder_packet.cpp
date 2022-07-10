/**
 * @file encoder_packet.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <avcodec_datatype.h>
#include <encoder_packet.h>

#include <sunshine_util.h>


namespace encoder
{


    Packet* 
    packet_init()
    {
        Packet* pk = malloc(sizeof(Packet));
        pk->packet = av_packet_alloc();
        pk->replacement_array = LIST_OBJECT_CLASS->init();
    }

    void    
    packet_finalize(Packet* packet)
    {
        av_packet_unref(packet->packet);
        array_object_finalize(packet->replacement_array);
    }


    PacketClass*        
    packet_class_init()
    {
        static bool initialize = false;
        static PacketClass klass;
        if (initialize)
            return &klass;
        
        initialize = true;
        return &klass;
    }
    
} // namespace encoder
