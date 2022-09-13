/**
 * @file gpu_cursor.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __GPU_CURSOR_H__
#define __GPU_CURSOR_H__

#include <d3d11_datatype.h>
#include <platform_common.h>

namespace gpu
{    
    typedef struct _GpuCursor {
        platf::Cursor base;
        d3d11::Texture2D texture;
        d3d11::ShaderResourceView input_res;
        D3D11_VIEWPORT cursor_view;
    }GpuCursor;


    platf::CursorClass*     gpu_cursor_class_init       ();
} // namespace gpu


#endif