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
#include <encoder_packet.h>
#include <encoder_thread.h>
#include <sunshine_util.h>

#include <sunshine_rtp.h>
#include <sunshine_config.h>

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>


extern "C" {
// this function implementation has already been defined in avio_internal.h: line 186
// we breaked the rule to use this

/**
 * Open a write only packetized memory stream with a maximum packet
 * size of 'max_packet_size'.  The stream is stored in a memory buffer
 * with a big-endian 4 byte header giving the packet size in bytes.
 *
 * @param s new IO context
 * @param max_packet_size maximum packet size (must be > 0)
 * @return zero if no error.
 */
int ffio_open_dyn_packet_buf(AVIOContext **, int);
}

#include <thread>

using namespace std::literals;

namespace rtp
{
    typedef struct sockaddr_in SocketAddress;

    typedef struct _UDPConn{
        unsigned short port;
        SocketAddress receiver;
        SOCKET rtpSocket;
        int mtu;
        bool active;
    }UDPConn;


    typedef struct _Video {
        UDPConn conn;
    }Video;

    typedef struct _BroadcastContext {
        std::thread video_thread;

        util::Broadcaster* shutdown_event;

        util::QueueArray* packet_queue;

        Video video;
    }BroadcastContext;



    static bool
    rtp_open_internal(UDPConn* conn) 
    {
        memset(&conn->receiver,0, sizeof(SocketAddress));
        conn->rtpSocket= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if(conn->rtpSocket < 0)
            return FALSE;

        conn->receiver.sin_family = AF_INET;
        conn->receiver.sin_port = htons(conn->port); 
        conn->receiver.sin_addr.s_addr = inet_addr("localhost"); 
        conn->active = TRUE;
        return TRUE;
    }

    static int
    rtp_close_ports(BroadcastContext* ctx) 
    {
        //TODO
        // if(ctx->video.conn.port != 0)
            // fclose(ctx->video.conn.port);

        return 0;
    }

    int
    rtp_open_ports(BroadcastContext* ctx) 
    {
        // initialized?
        if(ctx->video.conn.active)
            return 0;

        if(!rtp_open_internal(&ctx->video.conn))
            return -1;

        LOG_INFO("Opened RTP port");
        return 0;
    }



    /**
     * @brief 
     * 
     * @param ctx 
     * @param sin 
     * @param codec
     * @return int 
     */
    static int
    rtp_stream_run_first_time(BroadcastContext *ctx, 
                              encoder::Session* session) 
    {
        if(ffio_open_dyn_packet_buf(&session->format_context->pb, session->format_context->packet_size) < 0) {
            LOG_ERROR("cannot open dynamic packet buffer\n");
            return -1;
        }

        if(rtp_open_ports(ctx) < 0) {
            LOG_ERROR("RTP: open ports failed");
            return -1;
        }

        // write header
        if(avformat_write_header(session->format_context, NULL) < 0) {
            LOG_ERROR("Cannot write stream");
            return -1;
        }

        byte* dummybuf;
        avio_close_dyn_buf(session->format_context->pb, &dummybuf);
        av_free(dummybuf);
        return 0;
    }


    /**
     * @brief 
     * 
     * @param ctx 
     * @param streamid 
     * @param buf 
     * @param buflen 
     * @return int 
     */
    int
    rtp_write_bindata(BroadcastContext *ctx, 
                      util::Buffer* buf) 
    {
        int i, pktlen, buflen;
        byte* data = (byte*)BUFFER_CLASS->ref(buf,&buflen);
        SocketAddress sin;

        if(!data)
            return 0;
        if(buflen < 4)
            return buflen;

        memcpy(&sin,&ctx->video.conn.receiver, sizeof(SocketAddress));

        
        // XXX: buffer is the reuslt from avio_open_dyn_buf.
        // Multiple RTP packets can be placed in a single buffer.
        // Format == 4-bytes (big-endian) packet size + packet-data
        // |----|--------------|----|-----------------------------------|
        //  val1 <------val--->
        // 4-bytes
        i = 0;
        while(i < buflen) {
            // calculate pktsize
            pktlen  = (data[i+0] << 24);
            pktlen += (data[i+1] << 16);
            pktlen += (data[i+2] << 8);
            pktlen += (data[i+3]);
            
            // continue for empty packet
            if(pktlen == 0) {
                i += 4;
                continue;
            }

            sendto(ctx->video.conn.rtpSocket, (const char*) &data[i+4], pktlen, 0, 
                (sockaddr*)((pointer)&sin), sizeof(SocketAddress));
            i += (4+pktlen);
        }
        BUFFER_CLASS->unref(buf);
        return i;
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


        while(QUEUE_ARRAY_CLASS->peek(packets)) {
            util::Buffer* video_packet_buffer = QUEUE_ARRAY_CLASS->pop(packets);
            if(IS_INVOKED(shutdown_event))
                break;



            int size;
            encoder::Packet* video_packet = (encoder::Packet*)BUFFER_CLASS->ref(video_packet_buffer,&size);
            if(size != sizeof(encoder::Packet)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            encoder::Session* session = (encoder::Session*)BUFFER_CLASS->ref(video_packet->session,&size);
            if(size != sizeof(encoder::Session)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            libav::Packet* av_packet = (libav::Packet*)BUFFER_CLASS->ref(video_packet->packet,&size);
            if(size != sizeof(libav::Packet)) {
                LOG_ERROR("wrong datatype");
                continue;
            }

            static bool init = false;
            if(!init) 
                rtp_stream_run_first_time(ctx,session);

            init = true;

            if(av_packet->pts != (int64) AV_NOPTS_VALUE) {
                av_packet->pts = av_rescale_q(session->pts,
                        session->context->time_base,
                        session->stream->time_base);
            }
            // TODO
            if(ffio_open_dyn_packet_buf(&session->format_context->pb,session->format_context->packet_size) < 0){
                LOG_ERROR("buffer allocation failed");
                goto done;
            }           
            if(av_write_frame(session->format_context, av_packet) != 0) {
                LOG_ERROR("write failed");
                goto done;
            }

            int iolen;
            byte* iobuf;
            iolen = avio_close_dyn_buf(session->format_context->pb, &iobuf);
            util::Buffer* avbuf = BUFFER_CLASS->init((pointer)iobuf,iolen,av_free);

            if(rtp_write_bindata(ctx,avbuf) < 0) {
                LOG_ERROR("RTP write failed.");
                goto done;
            }


            BUFFER_CLASS->unref(avbuf);
            BUFFER_CLASS->unref(video_packet_buffer);
        }
    done:
        RAISE_EVENT(shutdown_event);
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
        ctx.video.conn.port = ENCODER_CONFIG->rtp.port;
        ctx.packet_queue = packet_queue;
        ctx.shutdown_event = shutdown_event;
        ctx.video_thread = std::thread { videoBroadcastThread, &ctx};
    }
} // namespace rtp
