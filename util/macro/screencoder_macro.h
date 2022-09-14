/**
 * @file screencoder_macro.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_MACRO_H__
#define __SUNSHINE_MACRO_H__
#include <screencoder_log.h>

#define DEFAULT_BITRATE ( 1000 * 1000 * 30 )
#define INFINITE 100000

#define LOG_ERROR(content)  error::log(__FILE__,__LINE__,"error",content)

#define LOG_WARNING(content)  error::log(__FILE__,__LINE__,"warning",content)

#define LOG_DEBUG(content)  error::log(__FILE__,__LINE__,"debug",content)

#define LOG_INFO(content)  error::log(__FILE__,__LINE__,"info",content)

#define FILTER_ERROR(buffer)  (error::Error)((long long)buffer < error::Error::ENCODER_ERROR_MAX && (long long)buffer > error::Error::ENCODER_ERROR_MIN ? (long long)buffer : 0)

#define NEW_ERROR(x)  if(x < error::Error::ENCODER_ERROR_MAX && x > error::Error::ENCODER_ERROR_MIN) { return(util::Buffer*)x; } else { LOG_ERROR("invalid error"); return(util::Buffer*)x;}

#define RETURN_PTR_ONCE(x)  { static bool init = false; if(init) { return &x; } else { init = true; } }

#define RETURN_ONCE(x)  { static bool init = false; if(init) { return x; } else { init = true; } }

#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#define DO_NOTHING do_nothing

void do_nothing(void*);

bool string_compare(char* a, char* b);
#endif