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
#include <boost/asio.hpp>

extern "C" {
#include <RtpAudioQueue.h>
#include <Video.h>
#include <rs.h>
}



namespace rtp
{
    typedef boost::asio::ip::udp::endpoint Endpoint;
    typedef boost::asio::ip::udp::socket   Socket;
    typedef boost::asio::ip::udp           Udp;
    typedef boost::asio::io_service        IO;
    typedef boost::system::error_code      Error;

    typedef struct _Video {
        int lowseq;
        Endpoint peer;
        util::QueueArray* idr_event;
    }Video;

    typedef struct _VideoPacketRaw {
        RTP_PACKET rtp;

        char reserved[4];

        NV_VIDEO_PACKET packet;
    }VideoPacketRaw;

    struct _Session {
        Config config;

        std::thread videoThread;

        util::QueueArray* mail;

        util::Broadcaster* shutdown_event;

        State state;
    };

    typedef struct _BroadcastContext {
        std::thread video_thread;

        IO io;

        Socket video_sock{io};
    }BroadcastContext;
    
    /**
     * @brief 
     * 
     * @param sock 
     */
    void 
    videoBroadcastThread(Socket *sock) 
    {
        util::QueueArray* packets;
        util::Broadcaster* shutdown_event;

        while(QUEUE_ARRAY_CLASS->peek(packets)) {
            if(IS_INVOKED(shutdown_event))
                break;

            util::Object* obj = QUEUE_ARRAY_CLASS->pop(packets);
            encoder::Packet* video_packet = (encoder::Packet*)OBJECT_CLASS->ref(obj);

            auto session = (Session*)video_packet->user_data;
            auto lowseq  = session->video.lowseq;


            libav::Packet* av_packet = (libav::Packet*)video_packet->packet;

            pointer payload = (pointer)av_packet;
            pointer payload_new;

            pointer nv_packet_header = "\0017charss";

            // TODO
            std::copy(std::begin(nv_packet_header), std::end(nv_packet_header), std::back_inserter(payload_new));
            std::copy(std::begin(payload), std::end(payload), std::back_inserter(payload_new));

            payload = payload_new;

            if(av_packet->flags & AV_PKT_FLAG_KEY) {
                while(LIST_OBJECT_CLASS->has_data(video_packet->replacement_array,0)) {
                    util::Object* obj = LIST_OBJECT_CLASS->get_data(video_packet->replacement_array,0);

                    encoder::Replace* replacement = (encoder::Replace*) OBJECT_CLASS->ref(obj);
                    pointer frame_old = replacement.old;
                    pointer frame_new = replacement._new;

                    // TODO : reimplement replace
                    payload_new = replace(payload, frame_old, frame_new);
                    payload     = { (char *)payload_new.data(), payload_new.size() };
                }
            }

            // insert packet headers
            auto blocksize         = session->config.packetsize + MAX_RTP_HEADER_SIZE;
            auto payload_blocksize = blocksize - sizeof(VideoPacketRaw);

            payload_new = insert(sizeof(VideoPacketRaw), payload_blocksize, payload, [&](void *p, int fecIndex, int end) {
                (VideoPacketRaw*) video_packet = (VideoPacketRaw*)p;
                video_packet->packet.flags = FLAG_CONTAINS_PIC_DATA;
            });

            payload = payload_new;

            try 
            {
                sock.send_to(asio::buffer(payload), session->video.peer);
                session->video.lowseq = lowseq;
            } catch(const std::exception &e) {
                LOG_ERROR(e.what());
                std::this_thread::sleep_for(100ms);
            }
            OBJECT_CLASS->unref(obj);
        }
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
        Error err;
        ctx->video_sock.open(Udp::v4(),ec);
        if(ec) {
            LOG_ERROR("could not open port");
            return -1;
        }
        ctx->video_thread = std::thread { videoBroadcastThread, ctx->video_sock }
        ctx->io.run();
    }
} // namespace rtp
