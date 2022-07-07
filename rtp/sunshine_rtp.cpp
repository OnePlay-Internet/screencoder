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
#include <sunshine_rtp.h>
#include <sunshine_macro.h>
#include <sunshine_queue.h>
#include <sunshine_log.h>
#include <sunshine_event.h>
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

        event::Broadcaster* shutdown_event;

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
        event::Broadcaster* shutdown_event;

        pointer data;
        object::Object* obj = OBJECT_CLASS->init(data,DO_NOTHING);
        while(QUEUE_ARRAY_CLASS->pop(packets,obj)) {
            if(WAIT_EVENT(shutdown_event))
                break;
            
            encoder::Packet* video_packet = (encoder::Packet)obj->data;

            auto session = (Session*)video_packet->user_data;
            auto lowseq  = session->video.lowseq;


            libav::Packet* av_packet = (libav::Packet*)video_packet->packet;

            byte* payload = (char*)av_packet;
            byte* payload_new;

            byte* nv_packet_header = "\0017charss";

            // TODO
            // std::copy(std::begin(nv_packet_header), std::end(nv_packet_header), std::back_inserter(payload_new));
            // std::copy(std::begin(payload), std::end(payload), std::back_inserter(payload_new));

            payload = payload_new;

            if(av_packet->flags & AV_PKT_FLAG_KEY) {
                int index = 0;
                while(array_object_has_data(video_packet->replacement_array,index)) {
                    encoder::Replace* replacement = (encoder::Replace*)array_object_get_data(video_packet->replacement_array,index);

                    pointer frame_old = replacement.old;
                    pointer frame_new = replacement._new;

                    // TODO
                    // payload_new = replace(payload, frame_old, frame_new);
                    // payload     = { (char *)payload_new.data(), payload_new.size() };

                    index++;
                }
            }

            // insert packet headers
            auto blocksize         = session->config.packetsize + MAX_RTP_HEADER_SIZE;
            auto payload_blocksize = blocksize - sizeof(VideoPacketRaw);

            payload_new = insert(sizeof(VideoPacketRaw), payload_blocksize,
            payload, [&](void *p, int fecIndex, int end) {
                video_packet_raw_t *video_packet = (VideoPacketRaw*)p;

                video_packet->packet.flags = FLAG_CONTAINS_PIC_DATA;
            });

            payload = payload_new;

            // With a fecpercentage of 255, if payload_new is broken up into more than a 100 data_shards
            // it will generate greater than DATA_SHARDS_MAX shards.
            // Therefore, we start breaking the data up into three seperate fec blocks.
            auto multi_fec_threshold = 90 * blocksize;

            // We can go up to 4 fec blocks, but 3 is plenty
            constexpr auto MAX_FEC_BLOCKS = 3;

            std::array<std::string_view, MAX_FEC_BLOCKS> fec_blocks;
            decltype(fec_blocks)::iterator
            fec_blocks_begin = std::begin(fec_blocks),
            fec_blocks_end   = std::begin(fec_blocks) + 1;

            auto lastBlockIndex = 0;
            if(payload.size() > multi_fec_threshold) {
                LOG_ERROR("Generating multiple FEC blocks");

                // Align individual fec blocks to blocksize
                auto unaligned_size = payload.size() / MAX_FEC_BLOCKS;
                auto aligned_size   = ((unaligned_size + (blocksize - 1)) / blocksize) * blocksize;

                // Break the data up into 3 blocks, each containing multiple complete video packets.
                fec_blocks[0] = payload.substr(0, aligned_size);
                fec_blocks[1] = payload.substr(aligned_size, aligned_size);
                fec_blocks[2] = payload.substr(aligned_size * 2);

                lastBlockIndex = 2 << 6;
                fec_blocks_end = std::end(fec_blocks);
            }
            else {
                BOOST_LOG(verbose) << "Generating single FEC block"sv;
                fec_blocks[0] = payload;
            }

            try {
                auto blockIndex = 0;
                std::for_each(fec_blocks_begin, fec_blocks_end, [&](std::string_view &current_payload) {
                    auto packets = (current_payload.size() + (blocksize - 1)) / blocksize;

                    for(int x = 0; x < packets; ++x) {
                        auto *inspect  = (video_packet_raw_t *)&current_payload[x * blocksize];
                        auto av_packet = packet->av_packet;

                        inspect->packet.frameIndex        = av_packet->pts;
                        inspect->packet.streamPacketIndex = ((uint32_t)lowseq + x) << 8;

                        // Match multiFecFlags with Moonlight
                        inspect->packet.multiFecFlags  = 0x10;
                        inspect->packet.multiFecBlocks = (blockIndex << 4) | lastBlockIndex;

                        if(x == 0) 
                            inspect->packet.flags |= FLAG_SOF;

                        if(x == packets - 1) 
                            inspect->packet.flags |= FLAG_EOF;
                        
                    }

                    auto shards = fec::encode(current_payload, blocksize, fecPercentage, session->config.minRequiredFecPackets);

                    // set FEC info now that we know for sure what our percentage will be for this frame
                    for(auto x = 0; x < shards.size(); ++x) {
                    auto *inspect = (video_packet_raw_t *)shards.data(x);

                    inspect->packet.fecInfo =
                        (x << 12 |
                        shards.data_shards << 22 |
                        shards.percentage << 4);

                    inspect->rtp.header         = 0x80 | FLAG_EXTENSION;
                    inspect->rtp.sequenceNumber = util::endian::big<uint16_t>(lowseq + x);

                    inspect->packet.multiFecBlocks = (blockIndex << 4) | lastBlockIndex;
                    inspect->packet.frameIndex     = av_packet->pts;
                    }

                    for(auto x = 0; x < shards.size(); ++x) {
                        sock.send_to(asio::buffer(shards[x]), session->video.peer);
                    }

                    ++blockIndex;
                    lowseq += shards.size();
                });

                session->video.lowseq = lowseq;
            } catch(const std::exception &e) 
            {
                BOOST_LOG(error) << "Broadcast video failed "sv << e.what();
                std::this_thread::sleep_for(100ms);
            }
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
