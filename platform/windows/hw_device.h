#ifndef __SUNSHINE_HW_DEVICE_H__
#define __SUNSHINE_HW_DEVICE_H__
#include <common.h>
#include <encoder_packet.h>
#include <d3d11_datatype.h>




namespace hwdevice
{        
    typedef struct _ImageD3D {
      platf::Image base;

      platf::Display* display;

      directx::d3d11::ShaderResourceView input_res;
      directx::d3d11::RenderTargetView scene_rt;

      directx::d3d11::Texture2D texture;
    }ImageD3D;

    typedef struct _D3D11Device {
      platf::HWDevice base;

      libav::Frame* hwframe;

      encoder::Color* color_p;

      directx::d3d11::Buffer* info_scene;
      directx::d3d11::Buffer* color_matrix;

      directx::d3d11::InputLayout input_layout;

      directx::d3d11::RenderTargetView nv12_Y_rt;
      directx::d3d11::RenderTargetView nv12_UV_rt;

      // The image referenced by hwframe
      // The resulting image is stored here.
      ImageD3D img;

      // Clear nv12 render target to black
      ImageD3D back_img;

      directx::d3d11::VertexShader convert_UV_vs;
      directx::d3d11::PixelShader convert_UV_ps;
      directx::d3d11::PixelShader convert_Y_ps;

      directx::d3d11::PixelShader scene_ps;

      directx::d3d11::VertexShader scene_vs;

      D3D11_VIEWPORT outY_view;
      D3D11_VIEWPORT outUV_view;

      DXGI_FORMAT format;

      directx::d3d11::DeviceContext device_ctx;
    }D3D11Device;


    
} // namespace hwdevice

#endif