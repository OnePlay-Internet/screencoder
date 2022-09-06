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



/**
 * @brief 
 * create a copy of ptr to mutex, lock mutex then unlock after function is done
 */
#define LOCK(x)   std::mutex* mutx_ptr = x; (*mutx_ptr).lock(); defer _(NULL, [mutx_ptr](...){(*mutx_ptr).unlock();} )

#endif