/**
 * @file gpu_hw_device.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __GPU_HW_DEVICE_H__
#define __GPU_HW_DEVICE_H__
#include <platform_common.h>
#include <d3d11_datatype.h>

#define D3D11DEVICE_CLASS    gpu::d3d11_device_class_init()


namespace gpu
{        
    typedef struct _ImageD3D {
      platf::Image base;


      d3d11::ShaderResourceView input_res;
      d3d11::RenderTargetView scene_rt;

      d3d11::Texture2D texture;
    }ImageGpu;

    typedef struct _D3D11Device {
      platf::Device base;

      platf::Color* color_p;

      d3d11::Buffer info_scene;
      d3d11::Buffer color_matrix;

      d3d11::InputLayout input_layout;

      d3d11::RenderTargetView nv12_Y_rt;
      d3d11::RenderTargetView nv12_UV_rt;

      // The resulting image is stored here.
      ImageGpu img;

      // Clear nv12 render target to black
      ImageGpu back_img;

      d3d11::VertexShader convert_UV_vs;   // shader to convert color matrix (described in hlsl file)
      d3d11::PixelShader convert_UV_ps;    // shader to convert color matrix (described in hlsl file)
      d3d11::PixelShader convert_Y_ps;     // shader to convert color matrix (described in hlsl file)
      d3d11::PixelShader scene_ps;         // shader to convert color matrix (described in hlsl file)
      d3d11::VertexShader scene_vs;        // shader to convert color matrix (described in hlsl file)


      D3D11_VIEWPORT outY_view;
      D3D11_VIEWPORT outUV_view;

      DXGI_FORMAT format;

      d3d11::DeviceContext device_ctx;
      d3d11::Device device;
    }GpuDevice;


    typedef struct _D3D11DeviceClass {
      platf::DeviceClass base;

      void (*init_view_port) (GpuDevice* self,
                              float width, 
                              float height);
    }GpuDeviceClass;


    GpuDeviceClass*       d3d11_device_class_init        ();
    
} // namespace gpu

#endif