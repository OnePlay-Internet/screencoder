/**
 * @file avcodec_datatype.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <avcodec_wrapper.h>


namespace libav
{

    void    
    packet_free_func(void* pk)
    {
        av_packet_unref((Packet*)pk);
    }

    void    
    frame_free_func(void* pk)
    {
        Frame* frame = (Frame*)pk;
        av_frame_free(&frame);
    }

} // namespace libav


