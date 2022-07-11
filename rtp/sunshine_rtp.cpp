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

    /**
     * @brief 
     * return character position which substring start inside string
     * @param string 
     * @param sub_string 
     * @return uint 
     */
    uint
    search(char* string, char* sub_string)
    {
        int ret = 0;
        while (*(string + ret))
        {
            int i = 0;
            while (*(sub_string + i) == *(string + ret + i))
            {
                if (!*(sub_string + i + 1))
                    return ret;
                i++;
            }
            ret++;
        }
        return ret;
    }
    
    char*
    replace(char* original, 
            char* old, 
            char* _new) 
    {
        uint replace_size = strlen(original) - strlen(old) + MAX(strlen(old),strlen(_new));
        char* replaced = (char*)malloc(replace_size);
        memset((void*)replace,0,replace_size);

        uint inserter = search(original,old);
        memcpy(replaced,original,inserter);
        uint origin_found = inserter;

        if(inserter != strlen(original)) {
            // std::copy(std::begin(_new), std::end(_new), std::back_inserter(replaced));
            // std::copy(next + old.size(), end, std::back_inserter(replaced));
            memcpy(replaced + inserter, _new, strlen(_new));
            inserter += strlen(_new);
            memcpy(replaced + inserter, original + strlen(old) + (size_t)origin_found, strlen(_new));
        }
        return replaced;
    }

    typedef void (*InsertAction) (pointer data);

    char*
    insert(uint64 insert_size, 
           uint64 slice_size, 
           char* data,
           InsertAction action) 
    {
        // auto pad      = strlen(data) % slice_size != 0;
        // auto elements = strlen(data) / slice_size + (pad ? 1 : 0);

        char* result = NULL;
        // (char*)malloc(elements * insert_size + (uint64)strlen(data));

        // char* next = data;
        // for(auto x = 0; x < elements - 1; ++x) {
        //     void *p = &result[x * (insert_size + slice_size)];
        //     action(p);

        //     memcpy(next, next + slice_size, (char *)p + insert_size);
        //     next += slice_size;
        // }

        // auto x  = elements - 1;
        // void *p = &result[x * (insert_size + slice_size)];

        // action(p);

        // std::copy(next, data + strlen(data), (char *)p + insert_size);

        return result;
    }

    void
    insert_action(pointer p)
    {
        VideoPacketRaw* video_packet = (VideoPacketRaw*)p;
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

            util::Object* obj = QUEUE_ARRAY_CLASS->pop(packets);
            encoder::Packet* video_packet = (encoder::Packet*)OBJECT_CLASS->ref(obj);

            auto session = (Session*)video_packet->user_data;
            auto lowseq  = session->video.lowseq;


            libav::Packet* av_packet = (libav::Packet*)video_packet->packet;


            uint  inserter = 0;
            char* payload = (char*)av_packet;
            char* payload_new;

            char* nv_packet_header = "\0017charss";


            // TODO
            // std::copy(std::begin(nv_packet_header), std::end(nv_packet_header), std::back_inserter(payload_new));
            // std::copy(std::begin(payload), std::end(payload), std::back_inserter(payload_new));

            memcpy(payload_new,nv_packet_header,strlen(nv_packet_header));
            inserter += strlen(nv_packet_header);
            memcpy(payload_new,payload,strlen(payload));
            inserter += strlen(payload);

            payload = payload_new;

            if(av_packet->flags & AV_PKT_FLAG_KEY) {
                while(LIST_OBJECT_CLASS->has_data(video_packet->replacement_array,0)) {
                    util::Object* obj = LIST_OBJECT_CLASS->get_data(video_packet->replacement_array,0);

                    encoder::Replace* replacement = (encoder::Replace*) OBJECT_CLASS->ref(obj);
                    pointer frame_old = OBJECT_CLASS->ref(replacement->old);
                    pointer frame_new = OBJECT_CLASS->ref(replacement->_new);

                    // TODO : reimplement replace
                    payload_new = replace((char*)payload, (char*)frame_old, (char*)frame_new);
                    payload     = payload_new;
                }
            }

            // insert packet headers
            auto blocksize         = session->config.packetsize + MAX_RTP_HEADER_SIZE;
            auto payload_blocksize = blocksize - sizeof(VideoPacketRaw);

            payload_new = insert(sizeof(VideoPacketRaw), payload_blocksize, payload, insert_action);
            payload = payload_new;

            try 
            {
                sock->send_to(boost::asio::buffer(std::string_view {payload,strlen(payload)}), session->video.peer);
                session->video.lowseq = lowseq;
            } catch(const std::exception &e) {
                LOG_ERROR((char*)e.what());
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
