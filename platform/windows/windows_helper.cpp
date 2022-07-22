#include <d3d11_datatype.h>
#include <display_vram.h>
#include <sunshine_util.h>

#include <d3dcompiler.h>
#include <directxmath.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}

#include <platform_common.h>
#include <hw_device.h>

namespace helper
{
    directx::d3d11::Buffer
    convert_to_d3d11_buffer(directx::d3d11::Device device, 
                util::Buffer* buffer) 
    {
      int size;
      pointer ptr = BUFFER_CLASS->ref(buffer,&size);
      D3D11_BUFFER_DESC buffer_desc {
        size,
        D3D11_USAGE_IMMUTABLE,
        D3D11_BIND_CONSTANT_BUFFER
      };

      D3D11_SUBRESOURCE_DATA init_data {
        ptr
      };

      directx::d3d11::Buffer buf_p;
      auto status = device->CreateBuffer(&buffer_desc, &init_data, &buf_p);
      if(status) {
        LOG_ERROR("Failed to create buffer");
        return NULL;
      }

      BUFFER_CLASS->unref(buffer);
      return buf_p;
    }

    directx::d3d11::BlendState
    make_blend(directx::d3d11::Device device, 
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

      directx::d3d11::BlendState blend;
      auto status = device->CreateBlendState(&bdesc, &blend);
      if(status) {
        LOG_ERROR("Failed to create blend state");
        return NULL;
      }

      return blend;
    }



    util::Buffer*
    make_cursor_image(util::Buffer* img_obj, 
                      DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info) 
    {
      int size;
      uint32* img_data = (uint32*)BUFFER_CLASS->ref(img_obj,&size);
      const uint32 black       = 0xFF000000;
      const uint32 white       = 0xFFFFFFFF;
      const uint32 transparent = 0;

      switch(shape_info.Type) {
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
          for (int i = 0; i < size; i++)
          {
            uint32 *pixel = (img_data + i);
            if(*pixel & 0xFF000000) {
              *pixel = transparent;
            }
          }
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
          BUFFER_CLASS->unref(img_obj);
          return BUFFER_CLASS->duplicate(img_obj);
        default:
          break;
      }

      shape_info.Height /= 2;

      BUFFER_MALLOC(ret,shape_info.Width * shape_info.Height * 4,cursor_img);

      auto bytes       = shape_info.Pitch * shape_info.Height;

      auto pixel_begin = (uint32 *)cursor_img;
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

            if(bottom_p < (uint32*)(cursor_img)) {
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

      BUFFER_CLASS->unref(img_obj);
      BUFFER_CLASS->lock(ret);
      BUFFER_CLASS->unref(ret);
      return ret;
    }

    directx::d3d::Blob 
    compile_shader(LPCSTR file, 
                   LPCSTR entrypoint, 
                   LPCSTR shader_model) 
    {
      directx::d3d::Blob msg_p = NULL;
      directx::d3d::Blob compiled_p;

      DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;

    // #ifndef NDEBUG
    //   flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    // #endif
      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;

      auto wFile  = converter.from_bytes(file);
      auto status = D3DCompileFromFile(wFile.c_str(), nullptr, nullptr, entrypoint, shader_model, flags, 0, &compiled_p, &msg_p);

      if(msg_p) {
        // BOOST_LOG(warning) << std::string_view { (const char *)msg_p->GetBufferPointer(), msg_p->GetBufferSize() - 1 };
        msg_p->Release();
      }

      if(status) {
        LOG_ERROR("Couldn't compile");
        return NULL;
      }

      return directx::d3d::Blob { compiled_p };
    }

    directx::d3d::Blob 
    compile_pixel_shader(LPCSTR file) {
      return compile_shader(file, "main_ps", "ps_5_0");
    }

    directx::d3d::Blob 
    compile_vertex_shader(LPCSTR file) {
      return compile_shader(file, "main_vs", "vs_5_0");
    }

    /**
     * @brief 
     * 
     * @param device 
     * @param shader_res 
     * @param render_target 
     * @param width 
     * @param height 
     * @param format 
     * @param tex 
     * @return int 
     */
    int 
    init_render_target_a(directx::d3d11::Device device, 
            hwdevice::ImageD3D* img,
            int width, int height, 
            DXGI_FORMAT format, 
            directx::d3d11::Texture2D tex) 
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc {
          format,
          D3D11_SRV_DIMENSION_TEXTURE2D
        };

        shader_resource_desc.Texture2D.MipLevels = 1;

        HRESULT status = device->CreateShaderResourceView(tex, &shader_resource_desc, &img->input_res);
        if(status) {
          LOG_ERROR("Failed to create render target texture for luma");
          return -1;
        }

        D3D11_RENDER_TARGET_VIEW_DESC render_target_desc {
          format,
          D3D11_RTV_DIMENSION_TEXTURE2D
        };

        status = device->CreateRenderTargetView(tex, &render_target_desc, &img->scene_rt);
        if(status) {
          LOG_ERROR("Failed to create render target view ");
          return -1;
        }

        return 0;
    }


    
} // namespace helper






namespace platf {
  
    Display* 
    display(MemoryType hwdevice_type, 
            char* display_name, 
            int framerate) 
    {
        if(hwdevice_type == MemoryType::dxgi) 
            return vram::display_class_init()->init(framerate, display_name);
        
        if(hwdevice_type == MemoryType::system)
            // TODO display ram
        
        return NULL;
    }

    char**
    display_names(MemoryType type) 
    {
      char** display_names;

      HRESULT status;

      //  "Detecting monitors...";


      directx::dxgi::Factory factory;
      status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **)&factory);
      if(FAILED(status)) {
        LOG_ERROR("Failed to create DXGIFactory1");
        return {};
      }

      directx::dxgi::Adapter adapter;
      for(int x = 0; factory->EnumAdapters1(x, &adapter) != DXGI_ERROR_NOT_FOUND; ++x) {
        DXGI_ADAPTER_DESC1 adapter_desc;
        adapter->GetDesc1(&adapter_desc);

        // BOOST_LOG(debug)
        //   << std::endl
        //   << "====== ADAPTER ====="sv << std::endl
        //   << "Device Name      : "sv << converter.to_bytes(adapter_desc.Description) << std::endl
        //   << "Device Vendor ID : 0x"sv << util::hex(adapter_desc.VendorId).to_string_view() << std::endl
        //   << "Device Device ID : 0x"sv << util::hex(adapter_desc.DeviceId).to_string_view() << std::endl
        //   << "Device Video Mem : "sv << adapter_desc.DedicatedVideoMemory / 1048576 << " MiB"sv << std::endl
        //   << "Device Sys Mem   : "sv << adapter_desc.DedicatedSystemMemory / 1048576 << " MiB"sv << std::endl
        //   << "Share Sys Mem    : "sv << adapter_desc.SharedSystemMemory / 1048576 << " MiB"sv << std::endl
        //   << std::endl
        //   << "    ====== OUTPUT ======"sv << std::endl;

        IDXGIOutput* output = NULL;
        for(int y = 0; adapter->EnumOutputs(y, &output) != DXGI_ERROR_NOT_FOUND; ++y) {
          DXGI_OUTPUT_DESC desc;
          output->GetDesc(&desc);

          char* name =  (char*)desc.DeviceName;
          long width  = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
          long height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;



          // BOOST_LOG(debug)
          //   << "    Output Name       : "sv << device_name << std::endl
          //   << "    AttachedToDesktop : "sv << (desc.AttachedToDesktop ? "yes"sv : "no"sv) << std::endl
          //   << "    Resolution        : "sv << width << 'x' << height << std::endl
          //   << std::endl;
          *(display_names + y) = name;
        }
      }
      return display_names;
    }

} // namespace platf
