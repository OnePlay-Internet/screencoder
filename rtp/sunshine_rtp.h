/**
 * @file sunshine_rtp.h
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


namespace rtp
{
    int start_broadcast(util::Broadcaster* shutdown_event,
                        util::QueueArray* packet_queue) ;
} // namespace rtp


#endif