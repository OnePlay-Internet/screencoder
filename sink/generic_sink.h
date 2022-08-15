/**
 * @file generic_sink.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-08-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __GENERIC_SINK_H___
#define __GENERIC_SINK_H___

#include <screencoder_util.h>
#include <thread>

namespace sink
{

    typedef void  (*EncoderContextPreset) (GenericSink* sink,
                                          encoder::EncodeContext* ctx);

    typedef char* (*SinkDescription)      (GenericSink* sink);

    typedef int   (*SinkStart)            (GenericSink* sink);

    typedef void  (*HandlePacket)         (GenericSink* sink,
                                            libav::Packet* pkt);

    struct _GenericSink {
        char* name;

        /**
         * @brief 
         * NULL terminated
         */
        util::KeyValue* options;


        EncoderContextPreset preset;
        SinkDescription describe;
        SinkStart start;
        HandlePacket handle;
    };

    typedef struct _BroadcastContext {
        std::thread video_thread;
        sink::GenericSink* sink;

        util::Broadcaster* shutdown_event;
        util::Broadcaster* join_event;

        util::QueueArray* packet_queue;
    }BroadcastContext;

    int             start_broadcast (sink::GenericSink* sink,
                                     util::Broadcaster* shutdown_event,
                                     util::QueueArray* packet_queue) ;
} // namespace sink


#endif

