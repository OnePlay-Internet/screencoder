#include <cmath>

#include <codecvt>
#include <sunshine_util.h>

#include <string.h>

#include <d3dcompiler.h>
#include <directxmath.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}


#include <d3d11_datatype.h>
#include <platform_common.h>
#include <display_base.h>
#include <display_vram.h>
#include <windows_helper.h>
#include <gpu_hw_device.h>
#include <thread>

#include <comdef.h>



using namespace std::literals;


namespace gpu {
    platf::Capture    display_vram_snapshot   (platf::Display* disp,
                                               platf::Image *img_base, 
                                               std::chrono::milliseconds timeout, 
                                               bool cursor_visible); 
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
                        platf::Image* img, 
                        platf::SnapshootCallback snapshot_cb, 
                        util::Buffer* data,
                        encoder::EncodeThreadContext* thread_ctx,
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
          auto status = display_vram_snapshot((platf::Display*)self,img,1000ms,cursor);
          switch(status) {
            case platf::Capture::reinit:
            case platf::Capture::error:
              return status;
            case platf::Capture::timeout:
              std::this_thread::sleep_for(1ms);
              continue;
            case platf::Capture::ok:
              return snapshot_cb(img,data,thread_ctx);
            default:
              LOG_ERROR("Unrecognized capture status"); 
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
      gpu::ImageGpu* img = (gpu::ImageGpu*)img_base;

      HRESULT status;

      DXGI_OUTDUPL_FRAME_INFO frame_info = {0};

      dxgi::Resource res;
      platf::Capture capture_status = DUPLICATION_CLASS->next_frame(&self->base.dup,&frame_info, timeout, &res);

      if(capture_status != platf::Capture::ok) {
        return capture_status;
      }

      const bool mouse_update_flag = frame_info.LastMouseUpdateTime.QuadPart != 0 || frame_info.PointerShapeBufferSize > 0;
      const bool frame_update_flag = frame_info.AccumulatedFrames != 0 || frame_info.LastPresentTime.QuadPart != 0;
      const bool update_flag       = mouse_update_flag || frame_update_flag;

      if(!update_flag) {
        return platf::Capture::timeout;
      }

      // todo
      if(frame_info.PointerShapeBufferSize > 0) {
        DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info {};
        BUFFER_MALLOC(img_object,frame_info.PointerShapeBufferSize,img_ptr);

        UINT dummy;
        status = self->base.dup.dup->GetFramePointerShape(frame_info.PointerShapeBufferSize, img_ptr, &dummy, &shape_info);
        if(FAILED(status)) {
          _com_error err(status);
          LPCTSTR errMsg = err.ErrorMessage();
          LOG_ERROR("Failed to get new pointer shape");
          return platf::Capture::error;
        }

        int cursor_size;
        util::Buffer* cursor_buf = helper::make_cursor_image(img_object, shape_info);
        BUFFER_CLASS->unref(img_object);

        void* cursor_img = BUFFER_CLASS->ref(cursor_buf,&cursor_size);

        D3D11_SUBRESOURCE_DATA data {
          cursor_img,
          4 * shape_info.Width,
          0
        };

        // Create texture for cursor
        D3D11_TEXTURE2D_DESC t {};
        t.Width            = shape_info.Width;

        t.Height           = cursor_size / data.SysMemPitch;
        t.MipLevels        = 1;
        t.ArraySize        = 1;
        t.SampleDesc.Count = 1;
        t.Usage            = D3D11_USAGE_DEFAULT;
        t.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        d3d11::Texture2D texture;
        auto status = self->base.device->CreateTexture2D(&t, &data, &texture);
        if(FAILED(status)) {
          LOG_ERROR("Failed to create mouse texture");
          return platf::Capture::error;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC desc {
          DXGI_FORMAT_B8G8R8A8_UNORM,
          D3D11_SRV_DIMENSION_TEXTURE2D
        };
        desc.Texture2D.MipLevels = 1;

        // Free resources before allocating on the next line.
        // TODO
        self->cursor.input_res->Release();
        self->cursor.input_res = NULL;

        status = self->base.device->CreateShaderResourceView(texture, &desc, &self->cursor.input_res);
        if(FAILED(status)) {
          LOG_ERROR("Failed to create cursor shader resource view");
          return platf::Capture::error;
        }

        GPU_CURSOR_CLASS->set_texture(&self->cursor,
                                      t.Width, 
                                      t.Height, 
                                      texture);
      }

      if(frame_info.LastMouseUpdateTime.QuadPart) {
        GPU_CURSOR_CLASS->set_pos(&self->cursor,
                                  frame_info.PointerPosition.Position.x, 
                                  frame_info.PointerPosition.Position.y, 
                                  frame_info.PointerPosition.Visible && cursor_visible);
      }

      if(frame_update_flag) {
        // src.reset();
        status = res->QueryInterface(IID_ID3D11Texture2D, (void **)&self->src);

        if(FAILED(status)) {
          LOG_ERROR("Couldn't query interface");
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


      self->base.base.klass = (platf::DisplayClass*)display_class_init();
      if(display::display_base_init(&self->base,framerate, display_name)) {
        free(self);
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
        free(self);
        return NULL;
      }


      helper::HLSL* hlsl = helper::init_hlsl();
      if(!hlsl)
      {
        LOG_ERROR("unable to initialize hlsl");
        free(self);
        return NULL;
      }

      status = self->base.device->CreateVertexShader(hlsl->scene_vs_hlsl->GetBufferPointer(), hlsl->scene_vs_hlsl->GetBufferSize(), nullptr, &self->scene_vs);
      if(status) {
        LOG_ERROR("Failed to create scene vertex shader");
        free(self);
        return NULL;
      }

      status = self->base.device->CreatePixelShader(hlsl->scene_ps_hlsl->GetBufferPointer(), hlsl->scene_ps_hlsl->GetBufferSize(), nullptr, &self->scene_ps);
      if(status) {
        LOG_ERROR("Failed to create scene pixel shader");
        free(self);
        return NULL;
      }

      self->blend_enable  = helper::make_blend(self->base.device, true);
      self->blend_disable = helper::make_blend(self->base.device, false);

      if(!self->blend_disable || !self->blend_enable) {
        free(self);
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
        DisplayVram* disp = (DisplayVram*)self;
        DUPLICATION_CLASS->finalize(&disp->base.dup);
        if(disp->scene_vs)
            disp->scene_vs->Release();
        if(disp->scene_ps)
            disp->scene_ps->Release();
        if(disp->sampler_linear)
            disp->sampler_linear->Release();
        if(disp->blend_disable)
            disp->blend_disable->Release();
        if(disp->blend_enable)
            disp->blend_enable->Release();
        if(disp->src)
            disp->src->Release();
        if(disp->base.adapter)
            disp->base.adapter->Release();
        if(disp->base.device)
            disp->base.device->Release();
        if(disp->base.device_ctx)
            disp->base.device_ctx->Release();
        if(disp->base.dup.dup)
            disp->base.dup.dup->Release();
        if(disp->base.factory)
            disp->base.factory->Release();
        if(disp->base.output)
            disp->base.output->Release();
        free(self);
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
    init_render_target(d3d11::Device device, 
            ImageGpu* img,
            int width, int height, 
            DXGI_FORMAT format, 
            d3d11::Texture2D tex) 
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

    platf::Image*
    display_vram_alloc_img(platf::Display* platf_disp) 
    {
      DisplayVram* disp= (DisplayVram*) platf_disp;
      gpu::ImageGpu* img = (gpu::ImageGpu*)malloc(sizeof(gpu::ImageGpu));
      platf::Image* img_base = (platf::Image*)img;

      display::DisplayBase* display_base = (display::DisplayBase*)disp;

      img_base->pixel_pitch = 4;
      img_base->row_pitch   = img_base->pixel_pitch * platf_disp->width;
      img_base->width       = platf_disp->width;
      img_base->height      = platf_disp->height;

      img->display     = platf_disp;

      uint8* dummy_data = (uint8*)malloc(img_base->row_pitch * platf_disp->height * sizeof(uint8));
      memset(dummy_data,0,img_base->row_pitch * platf_disp->height * sizeof(uint8));
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
        LOG_ERROR("Failed to create img buf texture");
        return nullptr;
      }

      if(init_render_target(disp->base.device, img,
                            platf_disp->width, platf_disp->height, 
                            display_base->format,
                            img->texture)) 
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
      gpu::ImageGpu* img = (gpu::ImageGpu*)img_base;

      if(img->texture) 
        return 0;
      
      img->base.row_pitch  = disp->width * 4;

      int* dummy_data = (int*)malloc(sizeof(int) * (disp->width * disp->height));
      D3D11_SUBRESOURCE_DATA data {
        (pointer)dummy_data,
        (UINT)img->base.row_pitch
      };
      memset(dummy_data,0, sizeof(int)* (disp->width * disp->height));

      D3D11_TEXTURE2D_DESC t {};
      t.Width            = disp->width;
      t.Height           = disp->height;
      t.MipLevels        = 1;
      t.ArraySize        = 1;
      t.SampleDesc.Count = 1;
      t.Usage            = D3D11_USAGE_DEFAULT;
      t.Format           = self->base.format;
      t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

      d3d11::Texture2D tex;
      auto status = self->base.device->CreateTexture2D(&t, &data, &tex);
      if(FAILED(status)) {
        LOG_ERROR("Failed to create dummy texture");
        return -1;
      }

      img->texture      = tex;
      img->base.data    = (byte*)img->texture;
      return 0;
    }

    platf::Device*
    display_vram_make_hwdevice(platf::Display* disp,
                               platf::PixelFormat pix_fmt) 
    {
      DisplayVram* self = (DisplayVram*) disp; 
      
      if(pix_fmt != platf::PixelFormat::nv12) {
        LOG_ERROR("DisplayVram doesn't support pixel format");
        return nullptr;
      }

      return D3D11DEVICE_CLASS->base.init((platf::Display*)self,
        self->base.device,
        self->base.device_ctx,
        pix_fmt);
    }


    
    
    DisplayVramClass*    
    display_class_init()
    {
        static bool init = false;
        static DisplayVramClass klass;
        if (init)
            return &klass;
        
        init = true;
        klass.base.init          = display_vram_init;
        klass.base.finalize      = display_vram_finalize;
        klass.base.alloc_img     = display_vram_alloc_img;
        klass.base.dummy_img     = display_vram_dummy_img;
        klass.base.make_hwdevice = display_vram_make_hwdevice;
        klass.base.snapshot      = display_vram_snapshot;
        klass.base.capture       = display_vram_capture;
        return &klass;
    }



} // namespace platf::dxgi