/**
 * @file generic_sink.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-08-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <generic_sink.h>
#include <screencoder_util.h>
#include <thread>
#include <screencoder_rtp.h>

using namespace std::literals;

namespace sink
{        
    
    /**
     * @brief 
     * 
     * @param sock 
     */
    void 
    videoBroadcastThread(BroadcastContext* ctx) 
    {
        util::QueueArray* packets = ctx->packet_queue;
        util::Broadcaster* shutdown_event = ctx->shutdown_event;


        while(true) {
            if(!QUEUE_ARRAY_CLASS->peek(packets)) {
                std::this_thread::sleep_for(100us); // decrease sleep interval cause cpu consumption ramp up
                continue;
            }

            if(IS_INVOKED(shutdown_event))
                break;

            int size;
            util::Buffer* video_packet_buffer;
            libav::Packet* av_packet = (libav::Packet*)QUEUE_ARRAY_CLASS->pop(packets,&video_packet_buffer,&size);
            if(size != sizeof(libav::Packet)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            ctx->sink->handle(ctx->sink,video_packet_buffer);          
            BUFFER_UNREF(video_packet_buffer);
        }

        RAISE_EVENT(shutdown_event);
        RAISE_EVENT(ctx->join_event);
    }


    /**
     * @brief 
     * 
     * @param ctx 
     * @return int 
     */
    int 
    start_broadcast(sink::GenericSink* sink,
                    util::Broadcaster* shutdown_event,
                    util::QueueArray* packet_queue) 
    {
        BroadcastContext ctx;
        ctx.packet_queue = packet_queue;
        ctx.shutdown_event = shutdown_event;
        ctx.join_event = NEW_EVENT;
        ctx.sink = sink;

        sink->start(sink);
        ctx.video_thread = std::thread { videoBroadcastThread, &ctx};
        WAIT_EVENT(ctx.join_event);
    }
    
} // namespace sink
