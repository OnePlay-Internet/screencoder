/**
 * @file app_sink.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-08-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __APP_SINK_H__
#define __APP_SINK_H__

#include <screencoder_util.h>
#include <generic_sink.h>

#include <chrono>

namespace appsink
{
    typedef struct _AppSink AppSink;
    struct _AppSink {
        sink::GenericSink base;

        util::QueueArray* out;

        std::chrono::high_resolution_clock::time_point prev_pkt_sent;
        std::chrono::high_resolution_clock::time_point this_pkt_sent;
    };


    sink::GenericSink*    new_app_sink    ();
} // namespace rtp



#endif