/**
 * @file sunshine_rtp.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_session.h>
#include <sunshine_util.h>

#include <sunshine_rtp.h>
#include <sunshine_config.h>

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>




#include <thread>

using namespace std::literals;

namespace rtp
{
    typedef struct _BroadcastContext {
        std::thread video_thread;

        util::Broadcaster* shutdown_event;
        util::Broadcaster* join_event;

        util::QueueArray* packet_queue;
    }BroadcastContext;

    RtpContext*
    make_rtp_context(encoder::EncodeContext* encode)
    {
        static bool init = false;
        static RtpContext ret;
        if(init)
            goto done;

        init = true;
        ret.format = avformat_alloc_context();
        ret.format->oformat = av_guess_format("rtp",NULL,NULL);
        snprintf(ret.format->filename, sizeof(ret.format->filename), 
            "rtp://%s:%d", "localhost", ENCODER_CONFIG->rtp.port);

        ret.stream = avformat_new_stream (ret.format, encode->codec);

        if (ret.format->oformat->flags & AVFMT_GLOBALHEADER)
            encode->context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;


        ret.format->streams[0] = ret.stream;
        if (avio_open(&ret.format->pb, ret.format->filename, AVIO_FLAG_WRITE) < 0){
            LOG_ERROR("Error opening output file");
            return NULL;
        }

        if (avformat_write_header(ret.format, NULL) < 0){
            LOG_ERROR("Error writing header");
            return NULL;
        }
        done:
        avcodec_parameters_from_context(ret.stream->codecpar,encode->context);
        if (ret.format->oformat->flags & AVFMT_GLOBALHEADER)
            encode->context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        return &ret;
    }



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


        while(TRUE) {
            if(!QUEUE_ARRAY_CLASS->peek(packets))
                continue;

            util::Buffer* video_packet_buffer = QUEUE_ARRAY_CLASS->pop(packets);
            if(IS_INVOKED(shutdown_event))
                break;



            int size;
            encoder::Session* session = (encoder::Session*)BUFFER_CLASS->ref(video_packet_buffer,&size);
            if(size != sizeof(encoder::Session)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            libav::Packet* av_packet = session->packet;
            if(!av_packet) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            // TODO
            if(av_write_frame(session->rtp->format, av_packet) != 0) {
                LOG_ERROR("write failed");
                break;
            }

            BUFFER_CLASS->unref(video_packet_buffer);
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
    start_broadcast(util::Broadcaster* shutdown_event,
                    util::QueueArray* packet_queue) 
    {
        BroadcastContext ctx;
        memset(&ctx,0,sizeof(BroadcastContext));
        ctx.packet_queue = packet_queue;
        ctx.shutdown_event = shutdown_event;
        ctx.join_event = NEW_EVENT;
        ctx.video_thread = std::thread { videoBroadcastThread, &ctx};
        WAIT_EVENT(ctx.join_event);
    }
} // namespace rtp
