/**
 * @file display_vram.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_DISPLAY_VRAM_H__
#define __SUNSHINE_DISPLAY_VRAM_H__
#include <display_base.h>
#include <gpu_cursor.h>
#include <d3d11_datatype.h>
#include <platform_common.h>

#define DISPLAY_VRAM_CLASS      gpu::display_class_init()

namespace gpu
{
    typedef struct _DisplayVram{
        display::DisplayBase base;

        d3d11::SamplerState sampler_linear;

        d3d11::BlendState blend_enable;
        d3d11::BlendState blend_disable;

        d3d11::PixelShader scene_ps;
        d3d11::VertexShader scene_vs;
    }DisplayVram;

    typedef struct _DisplayVramClass{
        platf::DisplayClass base;
    }DisplayVramClass;


    DisplayVramClass*    display_class_init    ();

} // namespace gpu

#endif