/**
 * @file cpu_cursor.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __CPU_CURSOR_H__
#define __CPU_CURSOR_H__


#include <screencoder_util.h>
#include <d3d11_datatype.h>

namespace cpu
{    
    typedef struct _Cursor {
        uint8* img_data;
        DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info;
        int x, y;
        bool visible;
    }Cursor;
    
} // namespace cpu



#endif