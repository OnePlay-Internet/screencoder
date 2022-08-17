/**
 * @file screencoder_rtp.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_session.h>
#include <screencoder_util.h>

#include <screencoder_rtp.h>
#include <screencoder_config.h>

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>




#include <thread>

using namespace std::literals;

namespace rtp
{


    void
    rtpsink_preset(sink::GenericSink* sink,
                   encoder::EncodeContext* encode)
    {
        rtp::RtpSink* rtp = (rtp::RtpSink*)sink;
        rtp->preseted = TRUE;
        rtp->stream = avformat_new_stream (rtp->format, encode->codec);
        avcodec_parameters_from_context(rtp->stream->codecpar,encode->context);
        if (rtp->format->oformat->flags & AVFMT_GLOBALHEADER)
            encode->context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    char*
    rtpsink_describe(sink::GenericSink* sink)
    {
        static char buf[2000];
        memset(buf,0,2000);

        rtp::RtpSink* rtp = (rtp::RtpSink*)sink;

        av_sdp_create(&rtp->format, 1, buf, 2000);
        return buf;
    }

    int
    rtpsink_start(sink::GenericSink* sink)
    {
        rtp::RtpSink* rtp = (rtp::RtpSink*)sink;

        while(!rtp->preseted) {
            std::this_thread::sleep_for(100ms);
        }

        rtp->format->streams[0] = rtp->stream;
        if (avio_open(&rtp->format->pb, rtp->format->filename, AVIO_FLAG_WRITE) < 0){
            LOG_ERROR("Error opening output file");
            return -1;
        }
    }

    void
    rtpsink_handle(sink::GenericSink* sink, 
                    libav::Packet* pkt)
    {
        RtpSink* rtp = (RtpSink*)sink;
        if(avformat_write_header(rtp->format,NULL) != 0) {
            LOG_ERROR("write header failed");
        }

        // TODO
        if(av_write_frame(rtp->format, pkt) != 0) {
            LOG_ERROR("write failed");
        }
    }

    void
    config_rtpsink(RtpSink* sink)
    {
        sink::GenericSink* base = (sink::GenericSink*)sink;
        sink->format = avformat_alloc_context();
        sink->format->oformat = av_guess_format("rtp",NULL,NULL);

        int i =0;
        while ((base->options+i) &&
               (base->options+i)->type == util::Type::INT && 
               string_compare((base->options+i)->key,"port") ){
            snprintf(sink->format->filename, sizeof(sink->format->filename), 
                "rtp://%s:%d", "localhost", (base->options+i)->int_value);
            i++;
        }
    }


    sink::GenericSink*    
    new_rtp_sink    ()
    {
        static bool init = FALSE;
        static RtpSink sink = {0};
        if(init)
            return (sink::GenericSink*)&sink;
        else
            init = TRUE;

        sink.base.name = "rtp";
        sink.base.options = util::new_keyvalue_pairs(2);
        util::keyval_new_intval(sink.base.options,"port",6000);

        sink.preseted = FALSE;
        sink.base.describe  = rtpsink_describe;
        sink.base.preset    = rtpsink_preset;
        sink.base.start     = rtpsink_start;
        sink.base.handle    = rtpsink_handle;

        config_rtpsink(&sink);
        return (sink::GenericSink*)&sink;
    }





} // namespace rtp
