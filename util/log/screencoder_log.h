/**
 * @file screencoder_error.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_ERROR_H__
#define __SUNSHINE_ERROR_H__


#include <chrono>


namespace error
{
    enum Error {
        ENCODER_ERROR_MIN = -100,

        CREATE_TEXTURE_FAILED,
        CREATE_HW_DEVICE_FAILED,
        CREATE_SESSION_FAILED,

        ENCODER_UNKNOWN,
        ENCODER_NEED_MORE_FRAME,
        ENCODER_END_OF_STREAM,

        ALLOC_IMG_ERR,

        ENCODER_ERROR_MAX,
        ERROR_NONE,
    };


    void  log(char* file,
              int line,
              char* level,
              char* message);

    typedef enum _BufferEventType {
        NONE,
        INIT,
        REF, 
        UNREF,
        FREE,
    }BufferEventType;

    typedef struct _BufferEvent {
        std::chrono::system_clock::time_point time;

        int line;

        char* file;

        BufferEventType type;
    }BufferEvent;

    typedef struct _BufferLog {
        BufferEvent events[100];

        char dataType[50];
    }BufferLog;

    void log_buffer(BufferLog* log,
                    std::chrono::system_clock::time_point created,
                    int line,
                    char* file,
                    BufferEventType type);
} // namespace error


#endif