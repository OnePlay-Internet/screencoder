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
#include <sunshine_util.h>

#include <sunshine_rtp.h>

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
        int lowseq;
        SocketAddress receiver;

        SOCKET rtpSocket;
        unsigned short port;

        int mtu;
        libav::FormatContext* context;

        util::Broadcaster* idr_event;
    }Video;

    typedef struct _BroadcastContext {
        std::thread video_thread;

        Video video;

        State state;
    }BroadcastContext;


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
        util::QueueArray* packets;
        util::Broadcaster* shutdown_event;


        while(QUEUE_ARRAY_CLASS->peek(packets)) {
            if(IS_INVOKED(shutdown_event))
                break;

            int iolen;
            byte* iobuf;

            int size;
            util::Buffer* video_packet_buffer = QUEUE_ARRAY_CLASS->pop(packets);
            encoder::Packet* video_packet = (encoder::Packet*)BUFFER_CLASS->ref(video_packet_buffer,&size);
            if(size != sizeof(encoder::Packet))
            {
                LOG_ERROR("wrong datatype");
                goto done;
            }

            Session* session = (Session*)video_packet->user_data;

            util::Buffer* av_packet_buffer = (util::Buffer*) video_packet->packet;
            libav::Packet* av_packet = (libav::Packet*)BUFFER_CLASS->ref(av_packet_buffer,NULL);


            // TODO
            if(ffio_open_dyn_packet_buf(ctx->video.context,ctx->video.mtu) < 0){
                LOG_ERROR("buffer allocation failed");
                goto done;
            }           
            if(av_write_frame(ctx->video.context, av_packet) != 0) {
                LOG_ERROR("write failed");
                goto done;
            }
            iolen = avio_close_dyn_buf(ctx->video.context, &iobuf);
            util::Buffer* avbuf = BUFFER_CLASS->init((pointer)iobuf,iolen,av_free);

            if(rtp_write_bindata(ctx,avbuf) < 0) {
                LOG_ERROR("RTP write failed.");
                goto done;
            }


            BUFFER_CLASS->unref(avbuf);
            BUFFER_CLASS->unref(av_packet_buffer);
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
    start_broadcast(BroadcastContext *ctx) 
    {
        Error ec;
        ctx->video_sock.open(Udp::v4(),ec);
        if(ec) {
            LOG_ERROR("could not open port");
            return -1;
        }
        ctx->video_thread = std::thread { videoBroadcastThread, &ctx->video_sock };
        ctx->io.run();
    }
} // namespace rtp
