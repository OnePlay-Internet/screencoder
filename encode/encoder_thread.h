/**
 * @file encoder_thread.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __ENCODER_THREAD_H__
#define __ENCODER_THREAD_H__

#include <screencoder_util.h>
#include <encoder_device.h>

#include <thread>

namespace encoder
{



    /**
     * @brief 
     * 
     * @param shutdown_event 
     * @param packet_queue 
     * @param config 
     * @param data 
     */
    void                capture          (platf::Display* capture,
                                          encoder::Encoder* encoder,
                                          encoder::Config* config,
                                          sink::GenericSink* sink,
                                          util::Broadcaster* shutdown_event,
                                          util::QueueArray* packet_queue);
    

    util::Buffer*       encode           (int frame_nr, 
                                          EncodeContext* session, 
                                          libav::Frame* frame);
} // namespace error


#endif