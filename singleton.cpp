/**
 * @file singleton.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-06-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <constant.h>
#include <singleton.h>
#include <thread_pool.h>
#include <video_sdk.h>



namespace singleton {
  video_capturer::video_capturer(){
  }
  video_capturer::~video_capturer(){

  }

  auto video_capturer::get_mail(mail::mail_id id) {
    switch (id)
    {
    case mail::SHUTDOWN:
        return man->queue<video::packet_t>()
    case mail::VIDEO_PACKETS:
        /* code */
        break;
    case mail::AUDIO_PACKETS:
        /* code */
        break;
    case mail::BROADCAST_SHUTDOWN:
        /* code */
        break;
    
    default:
        return nullptr;
    }
  }
} 