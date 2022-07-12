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

#ifndef __AVCODEC_DATATYPE_H__
#define __AVCODEC_DATATYPE_H__

extern "C" {
#include <libavcodec/avcodec.h>
}



namespace libav
{
    typedef AVHWDeviceType      HWDeviceType;

    typedef AVPacket            Packet;
    void    packet_free_func    (void* pk);

    typedef AVFrame             Frame;
    typedef AVCodecContext      CodecContext;
    typedef AVBufferRef         BufferRef;
    typedef AVPixelFormat       PixelFormat;
    typedef AVCodecID           CodecID;
} // namespace libav



#endif