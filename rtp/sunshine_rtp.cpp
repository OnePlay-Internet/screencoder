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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <thread>

using namespace std::literals;

namespace rtp
{
    typedef struct sockaddr_in SocketAddress;

    typedef struct _Video {
        SocketAddress receiver;
        SOCKET rtpSocket;

        unsigned short port;

        int mtu;

        util::Broadcaster* idr_event;
    }Video;

    typedef struct _BroadcastContext {
        std::thread video_thread;

        util::Broadcaster* shutdown_event;

        util::QueueArray* packet_queue;

        Video video;
    }BroadcastContext;


    static bool
    setup_receiver(Video* video)
    {
        // Declare variables
        SOCKET ListenSocket;
        SocketAddress saServer;
        hostent* localHost;
        char* localIP;

        // Create a listening socket
        ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        // Get the local host information
        localHost = gethostbyname("");
        localIP = inet_ntoa (*(struct in_addr *)*localHost->h_addr_list);

        // Set up the sockaddr structure
        saServer.sin_family = AF_INET;
        saServer.sin_addr.s_addr = inet_addr(localIP);
        saServer.sin_port = htons(5150);

        // Bind the listening socket using the
        // information in the sockaddr structure
        bind( ListenSocket,(SOCKADDR*) &saServer, sizeof(saServer) );
    }


    static int
    rtp_open_internal(unsigned short *port) 
    {
        SOCKET s;
        SocketAddress sin;
        int sinlen;
        bzero(&sin, sizeof(sin));
        if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
            return -1;
        sin.sin_family = AF_INET;
        if(bind(s, (SocketAddress*) &sin, sizeof(sin)) < 0) {
            close(s);
            return -1;
        }
        sinlen = sizeof(sin);
        if(getsockname(s, (SocketAddress*) &sin, &sinlen) < 0) {
            close(s);
            return -1;
        }
        *port = ntohs(sin.sin_port);
        return s;
    }

    static int
    rtp_close_ports(BroadcastContext* ctx) 
    {
        if(ctx->video.port != 0)
            close(ctx->video.port);

        return 0;
    }

    int
    rtp_open_ports(BroadcastContext* ctx) 
    {
        // initialized?
        if(ctx->video.rtpSocket != 0)
            return 0;

        ctx->video.rtpSocket = rtp_open_internal(&ctx->video.port);
        if(ctx->video.rtpSocket < 0)
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
        if(ffio_open_dyn_packet_buf(session->format_context->pb, ENCODER_CONFIG->mtu) < 0) {
            LOG_ERROR("cannot open dynamic packet buffer\n");
            return -1;
        }

        if(rtp_open_ports(ctx) < 0) {
            LOG_ERROR("RTP: open ports failed");
            return -1;
        }

        // write header
        if(avformat_write_header(ctx->video.context, NULL) < 0) {
            LOG_ERROR("Cannot write stream");
            return -1;
        }

        byte* dummybuf;
        avio_close_dyn_buf(ctx->video.context, &dummybuf);
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

        memcpy(&sin,&ctx->video.receiver, sizeof(sin));
        sin.sin_port = ctx->video.port;

        
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

            sendto(ctx->video.rtpSocket, (const char*) &data[i+4], pktlen, 0, 
                &sin, sizeof(SocketAddress));
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
            if(ffio_open_dyn_packet_buf(session->format_context->pb,ctx->video.mtu) < 0){
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
        BroadcastContext ctx = {0};
        ctx.packet_queue = packet_queue;
        ctx.shutdown_event = shutdown_event;
        ctx->video_thread = std::thread { videoBroadcastThread, &ctx};
    }
} // namespace rtp
