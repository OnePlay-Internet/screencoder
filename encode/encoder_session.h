/**
 * @file encoder_session.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __ENCODER_SESSION_H__
#define __ENCODER_SESSION_H__

#include <screencoder_util.h>
#include <encoder_datatype.h>

#include <platform_common.h>

namespace encoder
{    



    Session*            make_session    (Encoder* encoder, 
                                         int width, int height, 
                                         platf::Device* hwdevice); 

    
    void                session_finalize(pointer session);

 
    util::Buffer*
    make_session_buffer(platf::Image* img, 
                        Encoder* encoder,
                        platf::Display* display,
                        sink::GenericSink* sink);
} // namespace encoder

#endif