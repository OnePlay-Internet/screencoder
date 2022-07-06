/**
 * @file encoder_datatype.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __CAPTURE_DATATYPE_H__
#define __CAPTURE_DATATYPE_H__
#include <sunshine_datatype.h>

namespace capture
{
    typedef struct _Image {
        byte* data;
        int32 width;
        int32 height;
        int32 pixel_pitch;
        int32 row_pitch;
    }Image;

    typedef struct _CaptureContext CaptureContext;

} // namespace error


#endif