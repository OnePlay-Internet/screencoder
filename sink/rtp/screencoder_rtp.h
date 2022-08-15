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

#define RTP_SINK       rtp::new_rtp_sink()


namespace rtp
{
    struct _RtpContext {
        sink::GenericSink base;

        libav::Stream* stream;

        libav::FormatContext* format;
    };


    sink::GenericSink*    new_rtp_sink    ();



} // namespace rtp


#endif