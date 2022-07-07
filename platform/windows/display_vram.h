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
#include <display.h>
#include <d3d11_datatype.h>
#include <common.h>


namespace vram
{
    typedef struct _DisplayVram{
      display::DisplayBase base;
      display::GpuCursor cursor;

      directx::d3d11::SamplerState sampler_linear;

      directx::d3d11::BlendState blend_enable;
      directx::d3d11::BlendState blend_disable;

      directx::d3d11::PixelShader scene_ps;
      directx::d3d11::VertexShader scene_vs;

      directx::d3d11::Texture2D src;
    }DisplayVram;
    
} // namespace vram

#endif