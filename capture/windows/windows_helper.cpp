/**
 * @file windows_helper.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <windows_helper.h>
#include <d3d11_datatype.h>
#include <display_vram.h>
#include <screencoder_util.h>
#include <encoder_device.h>

#include <d3dcompiler.h>
#include <directxmath.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}

#include <platform_common.h>
#include <gpu_hw_device.h>
#include <thread>



// TODO: setup shader dir
#define SUNSHINE_SHADERS_DIR "../directx"

#define DISPLAY_RETRY   5
using namespace std::literals;

namespace helper
{
    d3d11::Buffer
    convert_to_d3d11_buffer(d3d11::Device device, 
                            util::Buffer* buffer) 
    {
      int size;
      pointer ptr = BUFFER_REF(buffer,&size);
      D3D11_BUFFER_DESC buffer_desc {
        size,
        D3D11_USAGE_IMMUTABLE,
        D3D11_BIND_CONSTANT_BUFFER
      };

      D3D11_SUBRESOURCE_DATA init_data {
        ptr
      };

      d3d11::Buffer buf_p;
      auto status = device->CreateBuffer(&buffer_desc, &init_data, &buf_p);
      if(status) {
        LOG_ERROR("Failed to create buffer");
        BUFFER_UNREF(buffer);
        return NULL;
      }

      BUFFER_UNREF(buffer);
      return buf_p;
    }

    d3d11::BlendState
    make_blend(d3d11::Device device, 
              bool enable) 
    {
      D3D11_BLEND_DESC bdesc {};
      auto &rt                 = bdesc.RenderTarget[0];
      rt.BlendEnable           = enable;
      rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

      if(enable) {
        rt.BlendOp      = D3D11_BLEND_OP_ADD;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;

        rt.SrcBlend  = D3D11_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

        rt.SrcBlendAlpha  = D3D11_BLEND_ZERO;
        rt.DestBlendAlpha = D3D11_BLEND_ZERO;
      }

      d3d11::BlendState blend;
      auto status = device->CreateBlendState(&bdesc, &blend);
      if(status) {
        LOG_ERROR("Failed to create blend state");
        return NULL;
      }

      return blend;
    }



    uint8*
    make_cursor_image(uint8* buffer,
                      int buffer_size,
                      DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info,
                      int* out_size) 
    {
      int size;
      uint32* img_data = (uint32*)buffer;
      const uint32 black       = 0xFF000000;
      const uint32 white       = 0xFFFFFFFF;
      const uint32 transparent = 0;

      if (shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR) {
          for (int i = 0; i < size; i++)
          {
            uint32 *pixel = (img_data + i);
            if(*pixel & 0xFF000000) {
              *pixel = transparent;
            }
          }
      } else if (shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) {
          uint8* ret = (uint8*)malloc(buffer_size);
          memcpy(ret,buffer,buffer_size);
          *out_size = buffer_size;
          return ret;
      }

      shape_info.Height /= 2;

      *out_size = shape_info.Width * shape_info.Height * 4;
      uint8* ret = (uint8*)malloc(*out_size);

      auto bytes       = shape_info.Pitch * shape_info.Height;

      auto pixel_begin = (uint32 *)ret;
      auto pixel_data  = pixel_begin;

      auto and_mask    = img_data;
      auto xor_mask    = img_data + bytes;

      for(auto x = 0; x < bytes; ++x) {
        for(auto c = 7; c >= 0; --c) {
          auto bit        = 1 << c;
          auto color_type = ((*and_mask & bit) ? 1 : 0) + ((*xor_mask & bit) ? 2 : 0);

          switch(color_type) {
          case 0: //black
            *pixel_data = black;
            break;
          case 2: //white
            *pixel_data = white;
            break;
          case 1: //transparent
          {
            *pixel_data = transparent;
            break;
          }
          case 3: //inverse
          {
            auto top_p    = pixel_data - shape_info.Width;
            auto left_p   = pixel_data - 1;
            auto right_p  = pixel_data + 1;
            auto bottom_p = pixel_data + shape_info.Width;

            // Get the x coordinate of the pixel
            auto column = (pixel_data - pixel_begin) % shape_info.Width != 0;

            if(top_p >= pixel_begin && *top_p == transparent) {
              *top_p = black;
            }

            if(column != 0 && left_p >= pixel_begin && *left_p == transparent) {
              *left_p = black;
            }

            if(bottom_p < (uint32*)(ret)) {
              *bottom_p = black;
            }

            if(column != shape_info.Width - 1) {
              *right_p = black;
            }
            *pixel_data = white;
          }
          }

          ++pixel_data;
        }
        ++and_mask;
        ++xor_mask;
      }

      return ret;
    }

    d3d::Blob 
    compile_shader(LPCSTR file, 
                   LPCSTR entrypoint, 
                   LPCSTR shader_model) 
    {
      d3d::Blob msg_p = NULL;
      d3d::Blob compiled_p;

      DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;

    // #ifndef NDEBUG
    //   flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    // #endif
      wchar_t wFile[1000] = {0};
      mbstowcs(wFile,file,strlen(file)); 
      auto status = D3DCompileFromFile(wFile, nullptr, nullptr, entrypoint, shader_model, flags, 0, &compiled_p, &msg_p);

      if(msg_p) {
        // BOOST_LOG(warning) << std::string_view { (const char *)msg_p->GetBufferPointer(), msg_p->GetBufferSize() - 1 };
        msg_p->Release();
      }

      if(status) {
        LOG_ERROR("Couldn't compile");
        return NULL;
      }

      return d3d::Blob { compiled_p };
    }

    d3d::Blob 
    compile_pixel_shader(LPCSTR file) {
      return compile_shader(file, "main_ps", "ps_5_0");
    }

    d3d::Blob 
    compile_vertex_shader(LPCSTR file) {
      return compile_shader(file, "main_vs", "vs_5_0");
    }




    platf::MemoryType
    map_dev_type(libav::HWDeviceType type) {
        switch(type) {
        case AV_HWDEVICE_TYPE_D3D11VA:
            return platf::MemoryType::dxgi;
        case AV_HWDEVICE_TYPE_VAAPI:
            return platf::MemoryType::vaapi;
        case AV_HWDEVICE_TYPE_CUDA:
            return platf::MemoryType::cuda;
        case AV_HWDEVICE_TYPE_NONE:
            return platf::MemoryType::system;
        default:
            return platf::MemoryType::unknown;
        }
        return platf::MemoryType::unknown;
    } 


    HLSL*
    init_hlsl() 
    {
      static HLSL hlsl = {0};
      RETURN_PTR_ONCE(hlsl);


      LOG_INFO("Compiling shaders...");
      hlsl.scene_vs_hlsl = helper::compile_vertex_shader(SUNSHINE_SHADERS_DIR "/SceneVS.hlsl");
      if(!hlsl.scene_vs_hlsl) {
        return NULL;
      }

      hlsl.convert_Y_ps_hlsl = helper::compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ConvertYPS.hlsl");
      if(!hlsl.convert_Y_ps_hlsl) {
        return NULL;
      }

      hlsl.convert_UV_ps_hlsl = helper::compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ConvertUVPS.hlsl");
      if(!hlsl.convert_UV_ps_hlsl) {
        return NULL;
      }

      hlsl.convert_UV_vs_hlsl = helper::compile_vertex_shader(SUNSHINE_SHADERS_DIR "/ConvertUVVS.hlsl");
      if(!hlsl.convert_UV_vs_hlsl) {
        return NULL;
      }

      hlsl.scene_ps_hlsl = helper::compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ScenePS.hlsl");
      if(!hlsl.scene_ps_hlsl) {
        return NULL;
      }
      LOG_INFO("Compiled shaders");
      return &hlsl;
    }    
} // namespace helper






namespace platf {
    Display* 
    get_display_by_name(encoder::Encoder* enc, 
                        char* display_name) 
    {
        if (!display_name) {
            char** display_names  = platf::display_names(helper::map_dev_type(enc->dev_type));
            display_name = *display_names;
        }
        

        platf::MemoryType hwdevice_type = helper::map_dev_type(enc->dev_type);
        if(hwdevice_type == MemoryType::dxgi) 
            return ((platf::DisplayClass*)DISPLAY_VRAM_CLASS)->init(display_name);
        
        if(hwdevice_type == MemoryType::system)
            // TODO display ram
        
        return NULL;
    }

    char**
    display_names(MemoryType type) 
    {
        static char* ret[10];
        static char display_names[10][100];
        memset(display_names,0,sizeof(char)*1000);

        HRESULT status;

        dxgi::Factory factory;
        status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **)&factory);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create DXGIFactory1");
            return {};
        }

        dxgi::Adapter adapter;
        for(int x = 0; factory->EnumAdapters1(x, &adapter) != DXGI_ERROR_NOT_FOUND; ++x) {
            DXGI_ADAPTER_DESC1 adapter_desc;
            adapter->GetDesc1(&adapter_desc);

            IDXGIOutput* output = NULL;
            for(int y = 0; adapter->EnumOutputs(y, &output) != DXGI_ERROR_NOT_FOUND; ++y) {
            DXGI_OUTPUT_DESC desc;
            output->GetDesc(&desc);

            char name[100] = {0};
            wcstombs(name, desc.DeviceName, 100);
            memcpy(display_names[x],name,strlen(name));
            }
        }

        for (int i = 0; i < 10; i++) {
            ret[i] = display_names[i];
        }
        
        return ret;
    }
} // namespace platf
