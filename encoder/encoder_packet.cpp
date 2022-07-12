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

#include <encoder_packet.h>

#include <sunshine_util.h>


namespace encoder
{
    Packet* 
    packet_init()
    {
        Packet* pk = (Packet*)malloc(sizeof(Packet));
        pk->replacement_array = LIST_OBJECT_CLASS->init();

        libav::Packet* av_packet = av_packet_alloc();
        pk->packet = BUFFER_CLASS->init(av_packet,sizeof(libav::Packet),libav::packet_free_func);
        return pk;
    }

    void    
    packet_finalize(void* packet)
    {
        Packet* self = ((Packet*)packet);
        BUFFER_CLASS->unref(self->packet);
        LIST_OBJECT_CLASS->finalize(self->replacement_array);
        free(packet);
    }


    PacketClass*        
    packet_class_init()
    {
        static bool initialize = false;
        static PacketClass klass;
        if (initialize)
            return &klass;
        
        klass.init = packet_init;
        klass.finalize = packet_finalize;
        initialize = true;
        return &klass;
    }
    
} // namespace encoder
