#include <cmath>

#include <codecvt>
#include <sunshine_macro.h>
#include <sunshine_datatype.h>
#include <avcodec_datatype.h>
#include <avcodec_datatype.h>

#include <encoder_packet.h>
#include <d3dcompiler.h>
#include <directxmath.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}


#include <d3d11_datatype.h>
#include <common.h>
#include <display.h>
#include <display_vram.h>
#include <windows_helper.h>
#include <hw_device.h>


#define SUNSHINE_SHADERS_DIR SUNSHINE_ASSETS_DIR "/shaders/directx"




namespace vram {
  /**
   * @brief 
   * 
   * @param snapshot_cb 
   * @param img 
   * @param cursor 
   * @return platf::Capture 
   */
  platf::Capture 
  display_vram_capture(DisplayVram* self,
                       platf::SnapshootCallback snapshot_cb, 
                       platf::Image img, 
                       bool *cursor) 
  {
    auto next_frame = std::chrono::steady_clock::now();

    while(img) {
      auto now = std::chrono::steady_clock::now();
      while(next_frame > now) {
        now = std::chrono::steady_clock::now();
      }
      next_frame = now + delay;

      auto status = snapshot(img.get(), 1000ms, *cursor);
      switch(status) {
      case platf::Capture::reinit:
      case platf::Capture::error:
        return status;
      case platf::Capture::timeout:
        std::this_thread::sleep_for(1ms);
        continue;
      case platf::Capture::ok:
        img = snapshot_cb(img);
        break;
      default:
        // BOOST_LOG(error) << "Unrecognized capture status ["sv << (int)status << ']';
        return status;
      }
    }

    return platf::Capture::ok;
  }

  platf::Capture 
  display_vram_snapshot(DisplayVram* self,
                        platf::Image *img_base, 
                        std::chrono::milliseconds timeout, 
                        bool cursor_visible) 
  {
    hwdevice::ImageD3D* img = (hwdevice::ImageD3D*)img_base;

    HRESULT status;

    DXGI_OUTDUPL_FRAME_INFO frame_info;

    directx::dxgi::Resource res_p {};
    platf::Capture capture_status = duplication_get_next_frame(dup,frame_info, timeout, &res_p);
    directx::dxgi::Resource res { res_p };

    if(capture_status != platf::Capture::ok) {
      return capture_status;
    }

    const bool mouse_update_flag = frame_info.LastMouseUpdateTime.QuadPart != 0 || frame_info.PointerShapeBufferSize > 0;
    const bool frame_update_flag = frame_info.AccumulatedFrames != 0 || frame_info.LastPresentTime.QuadPart != 0;
    const bool update_flag       = mouse_update_flag || frame_update_flag;

    if(!update_flag) {
      return platf::Capture::timeout;
    }

    if(frame_info.PointerShapeBufferSize > 0) {
      DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info {};

      byte* img_data = (byte*)malloc( frame_info.PointerShapeBufferSize );

      UINT dummy;
      status = dup.dup->GetFramePointerShape(img_data, img_data, &dummy, &shape_info);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to get new pointer shape [0x"sv << util::hex(status).to_string_view() << ']';
        return platf::Capture::error;
      }

      auto cursor_img = helper::make_cursor_image(img_data, shape_info);

      D3D11_SUBRESOURCE_DATA data {
        cursor_img,
        4 * shape_info.Width,
        0
      };

      // Create texture for cursor
      D3D11_TEXTURE2D_DESC t {};
      t.Width            = shape_info.Width;
      t.Height           = cursor_img.size() / data.SysMemPitch;
      t.MipLevels        = 1;
      t.ArraySize        = 1;
      t.SampleDesc.Count = 1;
      t.Usage            = D3D11_USAGE_DEFAULT;
      t.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
      t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

      texture2d_t texture;
      auto status = device->CreateTexture2D(&t, &data, &texture);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create mouse texture [0x"sv << util::hex(status).to_string_view() << ']';
        return platf::Capture::error;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC desc {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D3D11_SRV_DIMENSION_TEXTURE2D
      };
      desc.Texture2D.MipLevels = 1;

      // Free resources before allocating on the next line.
      cursor.input_res.reset();
      status = device->CreateShaderResourceView(texture.get(), &desc, &cursor.input_res);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create cursor shader resource view [0x"sv << util::hex(status).to_string_view() << ']';
        return platf::Capture::error;
      }

      cursor.set_texture(t.Width, t.Height, std::move(texture));
    }

    if(frame_info.LastMouseUpdateTime.QuadPart) {
      cursor.set_pos(frame_info.PointerPosition.Position.x, frame_info.PointerPosition.Position.y, frame_info.PointerPosition.Visible && cursor_visible);
    }

    if(frame_update_flag) {
      src.reset();
      status = res->QueryInterface(IID_ID3D11Texture2D, (void **)&self->src);

      if(FAILED(status)) {
        // BOOST_LOG(error) << "Couldn't query interface [0x"sv << util::hex(status).to_string_view() << ']';
        return platf::Capture::error;
      }
    }

    device_ctx->CopyResource(img->texture.get(), src.get());
    if(cursor.visible) {
      D3D11_VIEWPORT view {
        0.0f, 0.0f,
        (float)self->base.width, (float)self->base.height,
        0.0f, 1.0f
      };

      device_ctx->VSSetShader(scene_vs.get(), nullptr, 0);
      device_ctx->PSSetShader(scene_ps.get(), nullptr, 0);
      device_ctx->RSSetViewports(1, &view);
      device_ctx->OMSetRenderTargets(1, &img->scene_rt, nullptr);
      device_ctx->PSSetShaderResources(0, 1, &cursor.input_res);
      device_ctx->OMSetBlendState(blend_enable.get(), nullptr, 0xFFFFFFFFu);
      device_ctx->RSSetViewports(1, &cursor.cursor_view);
      device_ctx->Draw(3, 0);
      device_ctx->OMSetBlendState(blend_disable.get(), nullptr, 0xFFFFFFFFu);
    }

    return platf::Capture::ok;
  }

  int 
  display_vram_init(DisplayVram* self,
                    int framerate, 
                    char* display_name) {
    // if(display_base_init(framerate, display_name)) {
    //   return -1;
    // }

    D3D11_SAMPLER_DESC sampler_desc {};
    sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MinLOD         = 0;
    sampler_desc.MaxLOD         = D3D11_FLOAT32_MAX;

    auto status = device->CreateSamplerState(&self->sampler_desc, &self->sampler_linear);
    if(FAILED(status)) {
      BOOST_LOG(error) << "Failed to create point sampler state [0x"sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    status = device->CreateVertexShader(scene_vs_hlsl->GetBufferPointer(), scene_vs_hlsl->GetBufferSize(), nullptr, &scene_vs);
    if(status) {
      BOOST_LOG(error) << "Failed to create scene vertex shader [0x"sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    status = device->CreatePixelShader(scene_ps_hlsl->GetBufferPointer(), scene_ps_hlsl->GetBufferSize(), nullptr, &scene_ps);
    if(status) {
      BOOST_LOG(error) << "Failed to create scene pixel shader [0x"sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    blend_enable  = helper::make_blend(device.get(), true);
    blend_disable = helper::make_blend(device.get(), false);

    if(!blend_disable || !self->base.blend_enable) {
      return -1;
    }

    device_ctx->OMSetBlendState(blend_disable.get(), nullptr, 0xFFFFFFFFu);
    device_ctx->PSSetSamplers(0, 1, &sampler_linear);
    device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    return 0;
  }

  platf::Image*
  display_vram_alloc_img(DisplayVram* disp) 
  {
    platf::Image* img = (platf::Image*)malloc(sizeof(hwdevice::ImageD3D));
    platf::Display* platf_disp = (platf::Display*)disp;
    display::DisplayBase* display_base = (display::DisplayBase*)disp;

    img->pixel_pitch = 4;
    img->row_pitch   = img->pixel_pitch * platf_disp->width;
    img->width       = platf_disp->width;
    img->height      = platf_disp->height;
    img->display     = disp;

    auto dummy_data = std::make_unique<std::uint8_t[]>(img->row_pitch * height);
    D3D11_SUBRESOURCE_DATA data {
      dummy_data.get(),
      (UINT)img->row_pitch
    };
    std::fill_n(dummy_data.get(), img->row_pitch * platf_disp->height, 0);

    D3D11_TEXTURE2D_DESC t {};
    t.Width            = platf_disp->width;
    t.Height           = platf_disp->height;
    t.MipLevels        = 1;
    t.ArraySize        = 1;
    t.SampleDesc.Count = 1;
    t.Usage            = D3D11_USAGE_DEFAULT;
    t.Format           = disp->format;
    t.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    auto status = device->CreateTexture2D(&t, &data, &img->texture);
    if(FAILED(status)) {
      BOOST_LOG(error) << "Failed to create img buf texture [0x"sv << util::hex(status).to_string_view() << ']';
      return nullptr;
    }

    if(helper::init_render_target_b(device.get(), 
                            img->input_res, 
                            img->scene_rt, 
                            platf_disp->width, platf_disp->height, 
                            display_base->format, 
                            img->texture)) {
      return nullptr;
    }

    img->data = (std::uint8_t *)img->texture.get();

    return img;
  }

  /**
   * @brief 
   * 
   * @param img_base 
   * @return int 
   */
  int 
  display_vram_dummy_img(DisplayVram* self,
                         platf::Image *img_base) 
  {
    hwdevice::ImageD3D* img = (hwdevice::ImageD3D*)img_base;
    if(img->texture) {
      return 0;
    }

    img->row_pitch  = disp->base.width * 4;
    auto dummy_data = std::make_unique<int[]>(width * height);
    D3D11_SUBRESOURCE_DATA data {
      dummy_data.get(),
      (UINT)img->row_pitch
    };
    std::fill_n(dummy_data.get(), width * height, 0);

    D3D11_TEXTURE2D_DESC t {};
    t.Width            = width;
    t.Height           = height;
    t.MipLevels        = 1;
    t.ArraySize        = 1;
    t.SampleDesc.Count = 1;
    t.Usage            = D3D11_USAGE_DEFAULT;
    t.Format           = format;
    t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    directx::d3d11::Texture2D tex;
    auto status = device->CreateTexture2D(&t, &data, &tex);
    if(FAILED(status)) {
      BOOST_LOG(error) << "Failed to create dummy texture [0x"sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    img->texture = std::move(tex);
    img->data    = (std::uint8_t *)img->texture.get();

    return 0;
  }

  platf::HWDevice*
  display_vram_make_hwdevice(platf::PixelFormat pix_fmt) 
  {
    if(pix_fmt != platf::PixelFormat::nv12) {
      // BOOST_LOG(error) << "DisplayVram doesn't support pixel format ["sv << from_pix_fmt(pix_fmt) << ']';
      return nullptr;
    }

    platf::HWDevice hwdevice = {0};

    auto ret = hwdevice->init(
      shared_from_this(),
      device.get(),
      device_ctx.get(),
      pix_fmt);

    if(ret) {
      return nullptr;
    }

    return hwdevice;
  }

  HLSL*
  init_hlsl() 
  {
    // BOOST_LOG(info) << "Compiling shaders..."sv;

    static bool initialize = false;
    static HLSL hlsl = {0};
    if (initialize)
      return &hlsl;
    

    hlsl.scene_vs_hlsl = compile_vertex_shader(SUNSHINE_SHADERS_DIR "/SceneVS.hlsl");
    if(!hlsl.scene_vs_hlsl) {
      return -1;
    }

    hlsl.convert_Y_ps_hlsl = compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ConvertYPS.hlsl");
    if(!hlsl.convert_Y_ps_hlsl) {
      return -1;
    }

    hlsl.convert_UV_ps_hlsl = compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ConvertUVPS.hlsl");
    if(!hlsl.convert_UV_ps_hlsl) {
      return -1;
    }

    hlsl.convert_UV_vs_hlsl = compile_vertex_shader(SUNSHINE_SHADERS_DIR "/ConvertUVVS.hlsl");
    if(!hlsl.convert_UV_vs_hlsl) {
      return -1;
    }

    hlsl.scene_ps_hlsl = compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ScenePS.hlsl");
    if(!hlsl.scene_ps_hlsl) {
      return -1;
    }
    // BOOST_LOG(info) << "Compiled shaders"sv;
    return &hlsl;
  }
} // namespace platf::dxgi