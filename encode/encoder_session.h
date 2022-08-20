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
    /**
     * @brief 
     * 
     * @param encoder 
     * @param width 
     * @param height 
     * @param framerate 
     * @param hwdevice 
     * @return EncodeContext* 
     */
    EncodeContext*            make_encode_context           (Encoder* encoder, 
                                               int width, int height, int framerate,
                                               platf::Device* hwdevice); 

    
    void                free_encode_context     (pointer data);
 
    /**
     * @brief 
     * 
     * @param img 
     * @param encoder 
     * @param display 
     * @param sink 
     * @return util::Buffer* 
     */
    util::Buffer*       make_encode_context_buffer    (Encoder* encoder,
                                                platf::Display* display,
                                                sink::GenericSink* sink);

    /**
     * @brief 
     * 
     * @param context 
     * @return util::Buffer* 
     */
    util::Buffer*       make_avframe_buffer     (EncodeContext* context);
} // namespace encoder

#endif