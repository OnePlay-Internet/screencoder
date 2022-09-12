/**
 * @file screencoder_rtp.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_RTP_H__
#define __SUNSHINE_RTP_H__
#include <screencoder_util.h>
#include <generic_sink.h>
#include <chrono>

#define RTP_SINK(in,out)       rtp::new_rtp_sink(in,out)


namespace rtp
{
    struct _RtpSink {

        sink::GenericSink base;

        libav::Stream* stream;

        libav::FormatContext* format;

        util::QueueArray* sink_event_out;
        util::QueueArray* sink_event_in;

        std::chrono::high_resolution_clock::time_point prev_pkt_sent;
        std::chrono::high_resolution_clock::time_point this_pkt_sent;

        char url[100];

        bool preseted;
    };


    sink::GenericSink*  new_rtp_sink   (util::QueueArray* sink_event_in,
                                        util::QueueArray* sink_event_out);



} // namespace rtp


#endif