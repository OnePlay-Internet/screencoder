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
#include <libavformat/avformat.h>
}



namespace libav
{
    typedef AVHWDeviceType      HWDeviceType;

    typedef AVPacket            Packet;
    void    packet_free_func    (void* pk);

    typedef AVFrame             Frame;
    void    frame_free_func     (void* pk);

    typedef AVStream            Stream;

    typedef AVBufferRef         BufferRef;
    typedef AVPixelFormat       PixelFormat;

    typedef AVCodec             Codec;
    typedef AVCodecID           CodecID;
    typedef AVCodecContext      CodecContext;

    typedef AVOutputFormat      OutputFormat;
    typedef AVFormatContext     FormatContext;
} // namespace libav



#endif