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

#define GPU_CURSOR_CLASS        gpu::gpu_cursor_class_init()

namespace gpu
{    
    typedef struct _GpuCursor {
        platf::Cursor base;
        d3d11::Texture2D texture;
        d3d11::ShaderResourceView input_res;
        D3D11_VIEWPORT cursor_view;
    }GpuCursor;

    typedef struct _GpuCursorClass {
        void (*set_pos)     (GpuCursor* cursor,
                             LONG rel_x, 
                             LONG rel_y);

        void (*set_texture) (GpuCursor* cursor,
                             LONG width, 
                             LONG height, 
                             d3d11::Texture2D texture);
    }GpuCursorClass;

    GpuCursorClass*     gpu_cursor_class_init       ();
    
} // namespace gpu


#endif