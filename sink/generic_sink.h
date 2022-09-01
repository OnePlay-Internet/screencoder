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


    struct _GenericSink {
        char* name;

        /**
         * @brief 
         * NULL terminated
         */
        util::KeyValue* options;


        void  (*preset)     (GenericSink* sink,
                            encoder::EncodeContext* ctx);

        char* (*describe)   (GenericSink* sink);

        int   (*start)      (GenericSink* sink);

        void  (*stop)       (GenericSink* sink);

        void  (*handle)     (GenericSink* sink,
                            util::Buffer* pkt);
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

