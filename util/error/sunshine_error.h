/**
 * @file sunshine_error.h
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

namespace error
{
    typedef struct _Error {
        char* message;
        int code;
    }Error;

    Error* new_error(char* file,
                     int line,
                     char* level,
                     char* message);
} // namespace error


#endif