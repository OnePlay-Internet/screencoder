#include <cmath>

#include <codecvt>
#include <sunshine_util.h>

#include <string.h>

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
#include <encoder.h>
#include <thread>



using namespace std::literals;


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
    display_vram_capture(platf::Display* disp,
                        platf::SnapshootCallback snapshot_cb, 
                        platf::Image* img, 
                        bool cursor) 
    {
        DisplayVram* self = (DisplayVram*) disp; 
        auto next_frame = std::chrono::steady_clock::now();
        while(img) {
          auto now = std::chrono::steady_clock::now();
          while(next_frame > now) {
            now = std::chrono::steady_clock::now();
          }

          next_frame = now + self->base.delay;
          auto status = display_class_init()->snapshot((platf::Display*)self,img,1000ms,cursor);
          switch(status) {
            case platf::Capture::reinit:
            case platf::Capture::error:
              return status;
            case platf::Capture::timeout:
              std::this_thread::sleep_for(1ms);
              continue;
            case platf::Capture::ok:
              // TODO
              // snapshot_cb(img,self,encoder,);
              break;
            default:
              // BOOST_LOG(error) << "Unrecognized capture status ["sv << (int)status << ']';
              return status;
          }
        }
        return platf::Capture::ok;
    }

    platf::Capture 
    display_vram_snapshot(platf::Display* disp,
                          platf::Image *img_base, 
                          std::chrono::milliseconds timeout, 
                          bool cursor_visible) 
    {
      DisplayVram* self = (DisplayVram*) disp; 
      hwdevice::ImageD3D* img = (hwdevice::ImageD3D*)img_base;

      HRESULT status;

      DXGI_OUTDUPL_FRAME_INFO frame_info;

      directx::dxgi::Resource res;
      platf::Capture capture_status = duplication::duplication_class_init()->next_frame(&self->base.dup,frame_info, timeout, &res);

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

        BUFFER_MALLOC(img_object,frame_info.PointerShapeBufferSize,img_ptr);

        UINT dummy;
        status = self->base.dup.dup->GetFramePointerShape(img_object->size, img_ptr, &dummy, &shape_info);
        if(FAILED(status)) {
          // BOOST_LOG(error) << "Failed to get new pointer shape [0x"sv << util::hex(status).to_string_view() << ']';
          return platf::Capture::error;
        }

        byte* cursor_img = helper::make_cursor_image((byte*)img_ptr, shape_info);

        D3D11_SUBRESOURCE_DATA data {
          cursor_img,
          4 * shape_info.Width,
          0
        };

        // Create texture for cursor
        D3D11_TEXTURE2D_DESC t {};
        t.Width            = shape_info.Width;

        // TODO
        // t.Height           = strlen(cursor_img) / data.SysMemPitch;
        t.MipLevels        = 1;
        t.ArraySize        = 1;
        t.SampleDesc.Count = 1;
        t.Usage            = D3D11_USAGE_DEFAULT;
        t.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        directx::d3d11::Texture2D texture;
        auto status = self->base.device->CreateTexture2D(&t, &data, &texture);
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
        // TODO
        // self->cursor.input_res.reset();
        status = self->base.device->CreateShaderResourceView(texture, &desc, &self->cursor.input_res);
        if(FAILED(status)) {
          // BOOST_LOG(error) << "Failed to create cursor shader resource view [0x"sv << util::hex(status).to_string_view() << ']';
          return platf::Capture::error;
        }

        display::gpu_cursor_class_init()->set_texture(&self->cursor,
                                                      t.Width, 
                                                      t.Height, 
                                                      texture);
      }

      if(frame_info.LastMouseUpdateTime.QuadPart) {
        display::gpu_cursor_class_init()->set_pos(&self->cursor,
                                                  frame_info.PointerPosition.Position.x, 
                                                  frame_info.PointerPosition.Position.y, 
                                                  frame_info.PointerPosition.Visible && cursor_visible);
      }

      if(frame_update_flag) {
        // src.reset();
        status = res->QueryInterface(IID_ID3D11Texture2D, (void **)&self->src);

        if(FAILED(status)) {
          // BOOST_LOG(error) << "Couldn't query interface [0x"sv << util::hex(status).to_string_view() << ']';
          return platf::Capture::error;
        }
      }

      self->base.device_ctx->CopyResource(img->texture, self->src);
      if(self->cursor.visible) {
        D3D11_VIEWPORT view {
          0.0f, 0.0f,
          (float)self->base.base.width, 
          (float)self->base.base.height,
          0.0f, 1.0f
        };

        self->base.device_ctx->VSSetShader(self->scene_vs, nullptr, 0);
        self->base.device_ctx->PSSetShader(self->scene_ps, nullptr, 0);
        self->base.device_ctx->RSSetViewports(1, &view);
        self->base.device_ctx->OMSetRenderTargets(1, &img->scene_rt, nullptr);
        self->base.device_ctx->PSSetShaderResources(0, 1, &self->cursor.input_res);
        self->base.device_ctx->OMSetBlendState(self->blend_enable, nullptr, 0xFFFFFFFFu);
        self->base.device_ctx->RSSetViewports(1, &self->cursor.cursor_view);
        self->base.device_ctx->Draw(3, 0);
        self->base.device_ctx->OMSetBlendState(self->blend_disable, nullptr, 0xFFFFFFFFu);
      }

      return platf::Capture::ok;
    }

    platf::Display*
    display_vram_init(int framerate, 
                      char* display_name) 
    {
      DisplayVram* self = (DisplayVram*)malloc(sizeof(DisplayVram));
      memset(self,0,sizeof(DisplayVram));


      self->base.base.klass = display_class_init();
      if(display::display_base_init(&self->base,framerate, display_name)) {
        return NULL;
      }

      D3D11_SAMPLER_DESC sampler_desc {};
      sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      sampler_desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampler_desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampler_desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
      sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
      sampler_desc.MinLOD         = 0;
      sampler_desc.MaxLOD         = D3D11_FLOAT32_MAX;

      auto status = self->base.device->CreateSamplerState(&sampler_desc, &self->sampler_linear);
      if(FAILED(status)) {
        LOG_ERROR("Failed to create point sampler state");
        return NULL;
      }


      display::HLSL* hlsl = display::init_hlsl();
      status = self->base.device->CreateVertexShader(hlsl->scene_vs_hlsl->GetBufferPointer(), hlsl->scene_vs_hlsl->GetBufferSize(), nullptr, &self->scene_vs);
      if(status) {
        // BOOST_LOG(error) << "Failed to create scene vertex shader [0x"sv << util::hex(status).to_string_view() << ']';
        return NULL;
      }

      status = self->base.device->CreatePixelShader(hlsl->scene_ps_hlsl->GetBufferPointer(), hlsl->scene_ps_hlsl->GetBufferSize(), nullptr, &self->scene_ps);
      if(status) {
        // BOOST_LOG(error) << "Failed to create scene pixel shader [0x"sv << util::hex(status).to_string_view() << ']';
        return NULL;
      }

      self->blend_enable  = helper::make_blend(self->base.device, true);
      self->blend_disable = helper::make_blend(self->base.device, false);

      if(!self->blend_disable || !self->blend_enable) {
        return NULL;
      }

      self->base.device_ctx->OMSetBlendState(self->blend_disable, nullptr, 0xFFFFFFFFu);
      self->base.device_ctx->PSSetSamplers(0, 1, &self->sampler_linear);
      self->base.device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      return (platf::Display*)self;
    }

    void
    display_vram_finalize(void* self)
    {
        free(self);
    }

  

    platf::Image*
    display_vram_alloc_img(platf::Display* platf_disp) 
    {
      DisplayVram* disp= (DisplayVram*) platf_disp;
      hwdevice::ImageD3D* img = (hwdevice::ImageD3D*)malloc(sizeof(hwdevice::ImageD3D));
      platf::Image* img_base = (platf::Image*)img;

      display::DisplayBase* display_base = (display::DisplayBase*)disp;

      img_base->pixel_pitch = 4;
      img_base->row_pitch   = img_base->pixel_pitch * platf_disp->width;
      img_base->width       = platf_disp->width;
      img_base->height      = platf_disp->height;

      img->display     = platf_disp;

      BUFFER_MALLOC(dummy_obj,img_base->row_pitch * platf_disp->height,dummy_data);
      D3D11_SUBRESOURCE_DATA data {
        dummy_data,
        (UINT)img_base->row_pitch
      };

      D3D11_TEXTURE2D_DESC t {};
      t.Width            = platf_disp->width;
      t.Height           = platf_disp->height;
      t.MipLevels        = 1;
      t.ArraySize        = 1;
      t.SampleDesc.Count = 1;
      t.Usage            = D3D11_USAGE_DEFAULT;
      t.Format           = display_base->format;
      t.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

      auto status = disp->base.device->CreateTexture2D(&t, &data, &img->texture);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create img buf texture [0x"sv << util::hex(status).to_string_view() << ']';
        return nullptr;
      }

      if(helper::init_render_target_b(disp->base.device, 
                              img->input_res, 
                              img->scene_rt, 
                              platf_disp->width, platf_disp->height, 
                              display_base->format)) 
      {
        return nullptr;
      }

      img_base->data = (byte*)img->texture;
      return (platf::Image*)img;
    }

    /**
     * @brief 
     * 
     * @param img_base 
     * @return int 
     */
    int 
    display_vram_dummy_img(platf::Display* disp,
                          platf::Image *img_base) 
    {
      DisplayVram* self = (DisplayVram*) disp; 

      hwdevice::ImageD3D* img = (hwdevice::ImageD3D*)img_base;
      if(img->texture) 
        return 0;
      
      img->base.row_pitch  = disp->width * 4;

      byte* dummy_data = (byte*)malloc(disp->width * disp->height);
      memset(dummy_data,0, disp->width * disp->height);

      D3D11_SUBRESOURCE_DATA data {
        dummy_data,
        (UINT)img->base.row_pitch
      };

      D3D11_TEXTURE2D_DESC t {};
      t.Width            = disp->width;
      t.Height           = disp->height;
      t.MipLevels        = 1;
      t.ArraySize        = 1;
      t.SampleDesc.Count = 1;
      t.Usage            = D3D11_USAGE_DEFAULT;
      t.Format           = self->base.format;
      t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

      directx::d3d11::Texture2D tex;
      auto status = self->base.device->CreateTexture2D(&t, &data, &tex);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create dummy texture [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      img->texture = tex;
      img->base.data    = (byte*)img->texture;

      return 0;
    }

    platf::HWDevice*
    display_vram_make_hwdevice(platf::Display* disp,
                               platf::PixelFormat pix_fmt) 
    {
      DisplayVram* self = (DisplayVram*) disp; 
      
      if(pix_fmt != platf::PixelFormat::nv12) {
        // BOOST_LOG(error) << "DisplayVram doesn't support pixel format ["sv << from_pix_fmt(pix_fmt) << ']';
        return nullptr;
      }

      return hwdevice::d3d11_device_class_init()->base.init((platf::Display*)self,
        self->base.device,
        self->base.device_ctx,
        pix_fmt);
    }


    
    
    platf::DisplayClass*    
    display_class_init()
    {
        static bool init = false;
        static platf::DisplayClass klass;
        if (init)
            return &klass;
        
        init = true;
        klass.init          = display_vram_init;
        klass.finalize      = display_vram_finalize;
        klass.alloc_img     = display_vram_alloc_img;
        klass.dummy_img     = display_vram_dummy_img;
        klass.make_hwdevice = display_vram_make_hwdevice;
        klass.snapshot      = display_vram_snapshot;
        klass.capture       = display_vram_capture;
        return &klass;
    }



} // namespace platf::dxgi