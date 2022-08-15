/**
 * @file screencoder_util.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_UTIL_H___
#define __SUNSHINE_UTIL_H___

#include <avcodec_wrapper.h>
#include <screencoder_datatype.h>
#include <screencoder_object.h>
#include <screencoder_array.h>
#include <screencoder_queue.h>
#include <screencoder_macro.h>
#include <screencoder_log.h>
#include <screencoder_event.h>


namespace rtp {
    typedef struct _RtpSink RtpSink;

}
namespace sink {
    typedef struct _GenericSink          GenericSink;
}

namespace encoder {
typedef struct _EncodeContext EncodeContext;

typedef struct _Session Session;

typedef struct _EncodeThreadContext EncodeThreadContext;

typedef struct _Config  Config;

typedef struct _Encoder Encoder;
}


#endif