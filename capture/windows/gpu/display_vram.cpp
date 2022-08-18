#include <cmath>

#include <codecvt>
#include <screencoder_util.h>

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



    /**
     * @brief 
     * snapshoot screen by calling duplicationAPI next_frame method 
     * and render cursor texture2D to image
     * @param disp 
     * @param img_base 
     * @param timeout 
     * @param cursor_visible 
     * @return platf::Capture 
     */
    platf::Capture 
    display_vram_snapshot(platf::Display* disp,
                          platf::Image *img_base, 
                          std::chrono::milliseconds timeout, 
                          bool cursor_visible) 
    {
        DisplayVram* self = (DisplayVram*) disp; 
        display::DisplayBase* base = (display::DisplayBase*) disp; 

        gpu::ImageGpu* img = (gpu::ImageGpu*)img_base;

        HRESULT status;
        DXGI_OUTDUPL_FRAME_INFO frame_info = {0};

        dxgi::Resource res;
        platf::Capture capture_status = DUPLICATION_CLASS->next_frame(&base->dup,&frame_info, timeout, &res);

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
            status = base->dup.dup->GetFramePointerShape(frame_info.PointerShapeBufferSize, img_ptr, &dummy, &shape_info);
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
            auto status = base->device->CreateTexture2D(&t, &data, &texture);
            if(FAILED(status)) {
                LOG_ERROR("Failed to create mouse texture");
                return platf::Capture::error;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC desc {
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D3D11_SRV_DIMENSION_TEXTURE2D
            };
            desc.Texture2D.MipLevels = 1;

            // TODO
            // Free resources before allocating on the next line.
            self->cursor.input_res->Release();
            self->cursor.input_res = NULL;

            status = base->device->CreateShaderResourceView(texture, &desc, &self->cursor.input_res);
            if(FAILED(status)) {
                LOG_ERROR("Failed to create cursor shader resource view");
                return platf::Capture::error;
            }

            GPU_CURSOR_CLASS->set_texture(&self->cursor,
                                        t.Width, 
                                        t.Height, 
                                        texture);
            
            BUFFER_CLASS->unref(cursor_buf);
        }

        if(frame_info.LastMouseUpdateTime.QuadPart) {
            GPU_CURSOR_CLASS->set_pos(&self->cursor,
                                    frame_info.PointerPosition.Position.x, 
                                    frame_info.PointerPosition.Position.y, 
                                    frame_info.PointerPosition.Visible && cursor_visible);
        }

        if(frame_update_flag) {
            self->src->Release();
            status = res->QueryInterface(IID_ID3D11Texture2D, (void **)&self->src);
            if(FAILED(status)) {
                LOG_ERROR("Couldn't query interface");
                return platf::Capture::error;
            }
        }

        // copy texture from display to image
        base->device_ctx->CopyResource(img->texture, self->src);
        if(self->cursor.visible) {
            D3D11_VIEWPORT view {
                0.0f, 0.0f,
                (float)disp->width, 
                (float)disp->height,
                0.0f, 1.0f
            };

            base->device_ctx->VSSetShader(self->scene_vs, nullptr, 0);
            base->device_ctx->PSSetShader(self->scene_ps, nullptr, 0);
            base->device_ctx->RSSetViewports(1, &view);
            base->device_ctx->OMSetRenderTargets(1, &img->scene_rt, nullptr);
            base->device_ctx->PSSetShaderResources(0, 1, &self->cursor.input_res);
            base->device_ctx->OMSetBlendState(self->blend_enable, nullptr, 0xFFFFFFFFu);
            base->device_ctx->RSSetViewports(1, &self->cursor.cursor_view);
            base->device_ctx->Draw(3, 0);
            base->device_ctx->OMSetBlendState(self->blend_disable, nullptr, 0xFFFFFFFFu);
        }

        return platf::Capture::ok;
    }    
    
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
        platf::Capture status;
        while(img) {
            status = display_vram_snapshot(disp,img,1000ms,cursor);
            if(status == platf::Capture::error) {
                return status;
            } else if(status == platf::Capture::timeout) {
                std::this_thread::sleep_for(1ms);
            }


            status = snapshot_cb(&img,data,thread_ctx);
            if(status != platf::Capture::ok) 
                return status;
        }
        return platf::Capture::ok;
    }

    platf::Display*
    display_vram_init(char* display_name) 
    {
        DisplayVram* self = (DisplayVram*)malloc(sizeof(DisplayVram));
        display::DisplayBase* base = (display::DisplayBase*) self; 
        platf::Display* disp = (platf::Display*) self; 
        memset(self,0,sizeof(DisplayVram));


        memcpy(disp->name,display_name,strlen(display_name));
        disp->klass = (platf::DisplayClass*)display_class_init();
        if(display::display_base_init(&self->base,display_name)) {
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

        auto status = base->device->CreateSamplerState(&sampler_desc, &self->sampler_linear);
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

        status = base->device->CreateVertexShader(hlsl->scene_vs_hlsl->GetBufferPointer(), hlsl->scene_vs_hlsl->GetBufferSize(), nullptr, &self->scene_vs);
        if(status) {
            LOG_ERROR("Failed to create scene vertex shader");
            free(self);
            return NULL;
        }

        status = base->device->CreatePixelShader(hlsl->scene_ps_hlsl->GetBufferPointer(), hlsl->scene_ps_hlsl->GetBufferSize(), nullptr, &self->scene_ps);
        if(status) {
            LOG_ERROR("Failed to create scene pixel shader");
            free(self);
            return NULL;
        }

        self->blend_enable  = helper::make_blend(base->device, true);
        self->blend_disable = helper::make_blend(base->device, false);

        if(!self->blend_disable || !self->blend_enable) {
            free(self);
            return NULL;
        }

        base->device_ctx->OMSetBlendState(self->blend_disable, nullptr, 0xFFFFFFFFu);
        base->device_ctx->PSSetSamplers(0, 1, &self->sampler_linear);
        base->device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        return (platf::Display*)self;
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
    display_vram_alloc_img(platf::Display* disp) 
    {
        gpu::ImageGpu* img = (gpu::ImageGpu*)malloc(sizeof(gpu::ImageGpu));
        platf::Image* img_base = (platf::Image*)img;

        display::DisplayBase* display_base = (display::DisplayBase*)disp;

        img_base->pixel_pitch = 4;
        img_base->row_pitch   = img_base->pixel_pitch * disp->width;
        img->display     = disp;

        uint8* dummy_data = (uint8*)malloc(img_base->row_pitch * disp->height * sizeof(uint8));
        memset(dummy_data,0,img_base->row_pitch * disp->height * sizeof(uint8));
        D3D11_SUBRESOURCE_DATA data {
            dummy_data,
            (UINT)img_base->row_pitch
        };

        D3D11_TEXTURE2D_DESC t {};
        t.Width            = disp->width;
        t.Height           = disp->height;
        t.MipLevels        = 1;
        t.ArraySize        = 1;
        t.SampleDesc.Count = 1;
        t.Usage            = D3D11_USAGE_DEFAULT;
        t.Format           = display_base->format;
        t.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        auto status = display_base->device->CreateTexture2D(&t, &data, &img->texture);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create img buf texture");
            return nullptr;
        }

        if(init_render_target(display_base->device, img,
                                disp->width, disp->height, 
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
     * create empty texture 2d with frame size given from display
     * @param img_base 
     * @return int 
     */
    int 
    display_vram_dummy_img(platf::Display* disp,
                          platf::Image *img_base) 
    {
        display::DisplayBase* base = (display::DisplayBase*) disp; 
        DisplayVram* self = (DisplayVram*) disp; 

        gpu::ImageGpu* img = (gpu::ImageGpu*)img_base;

        if(img->texture) 
            return 0;
        
        img_base->row_pitch  = disp->width * 4;

        int* dummy_data = (int*)malloc(sizeof(int) * (disp->width * disp->height));
        D3D11_SUBRESOURCE_DATA data {
            (pointer)dummy_data,
            (UINT)img_base->row_pitch
        };
        memset(dummy_data,0, sizeof(int)* (disp->width * disp->height));

        D3D11_TEXTURE2D_DESC t {};
        t.Width            = disp->width;
        t.Height           = disp->height;
        t.MipLevels        = 1;
        t.ArraySize        = 1;
        t.SampleDesc.Count = 1;
        t.Usage            = D3D11_USAGE_DEFAULT;
        t.Format           = base->format;
        t.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        d3d11::Texture2D tex;
        auto status = base->device->CreateTexture2D(&t, &data, &tex);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create dummy texture");
            return -1;
        }

        img->texture      = tex;
        img_base->data    = (byte*)img->texture;
        return 0;
    }

    platf::Device*
    display_vram_make_hwdevice(platf::Display* disp,
                               platf::PixelFormat pix_fmt) 
    {
        display::DisplayBase* base = (display::DisplayBase*) disp; 
        DisplayVram* self = (DisplayVram*) disp; 
        
        if(pix_fmt != platf::PixelFormat::nv12) {
            LOG_ERROR("DisplayVram doesn't support pixel format");
            return nullptr;
        }


        if(base->dev)
            return base->dev;

        base->dev = ((platf::DeviceClass*)D3D11DEVICE_CLASS)->init((platf::Display*)self,
            base->device,
            base->device_ctx,
            pix_fmt);


        return base->dev;
    }


    
    
    DisplayVramClass*    
    display_class_init()
    {
        static DisplayVramClass klass;
        RETURN_PTR_ONCE(klass);
        
        klass.base.init          = display_vram_init;
        klass.base.alloc_img     = display_vram_alloc_img;
        klass.base.dummy_img     = display_vram_dummy_img;
        klass.base.make_hwdevice = display_vram_make_hwdevice;
        klass.base.capture       = display_vram_capture;
        return &klass;
    }
} // namespace platf::dxgi