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
#include <RtpAudioQueue.h>
#include <Video.h>
#include <rs.h>
}


#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <thread>

using namespace std::literals;

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
        util::Broadcaster* idr_event;
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

        Video video;
    };

    typedef struct _BroadcastContext {
        std::thread video_thread;

        IO io;

        Socket video_sock {io};
    }BroadcastContext;




    void
    insert_action(util::Buffer* buf,
                  int index, int max_index )
    {
        int size;
        VideoPacketRaw* video_packet = (VideoPacketRaw*)BUFFER_CLASS->ref(buf,&size);
        video_packet->packet.flags = FLAG_CONTAINS_PIC_DATA;
    }

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

            int size;
            util::Buffer* video_packet_buffer = QUEUE_ARRAY_CLASS->pop(packets);
            encoder::Packet* video_packet = (encoder::Packet*)BUFFER_CLASS->ref(video_packet_buffer,&size);
            if(size != sizeof(encoder::Packet))
            {
                LOG_ERROR("wrong datatype");
            }

            auto session = (Session*)video_packet->user_data;
            auto lowseq  = session->video.lowseq;


            util::Buffer* av_packet_buffer = (util::Buffer*) video_packet->packet;
            libav::Packet* av_packet = (libav::Packet*)BUFFER_CLASS->ref(av_packet_buffer,NULL);


            const char* nv_packet_header = "\0017charss";
            util::Buffer* nv_header = BUFFER_CLASS->init((pointer)nv_packet_header,strlen(nv_packet_header),DO_NOTHING);
            util::Buffer* payload = BUFFER_CLASS->merge(nv_header,av_packet_buffer);

            if(av_packet->flags & AV_PKT_FLAG_KEY) {
                while(LIST_OBJECT_CLASS->has_data(video_packet->replacement_array,0)) {
                    util::Buffer* replace_buffer= LIST_OBJECT_CLASS->get_data(video_packet->replacement_array,0);

                    int size;
                    encoder::Replace* replacement = (encoder::Replace*) BUFFER_CLASS->ref(replace_buffer,&size);
                    if(size != sizeof(encoder::Replace))
                    {
                        LOG_ERROR("wrong datatype");
                    }

                    // TODO : reimplement replace
                    util::Buffer* payload_new = BUFFER_CLASS->replace(payload, replacement->old, replacement->_new);
                    BUFFER_CLASS->unref(payload);
                    payload     = payload_new;
                }
            }
            BUFFER_CLASS->unref(av_packet_buffer);
            BUFFER_CLASS->unref(video_packet_buffer);


            {
                // insert packet headers
                int blocksize         = session->config.packetsize + MAX_RTP_HEADER_SIZE;
                int payload_blocksize = blocksize - sizeof(VideoPacketRaw);
                util::Buffer* payload_new = BUFFER_CLASS->insert((uint64)sizeof(VideoPacketRaw), (uint64)payload_blocksize, payload, insert_action);
                BUFFER_CLASS->unref(payload);
                payload = payload_new;
            }

            try 
            {
                std::string_view buf = std::string_view ((char*)payload,(int)BUFFER_CLASS->size(payload));
                sock->send_to(boost::asio::buffer(buf), session->video.peer);
                session->video.lowseq = lowseq;
            } catch(const std::exception &e) {
                LOG_ERROR((char*)e.what());
                std::this_thread::sleep_for(100ms);
            }

            BUFFER_CLASS->unref(payload);
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
