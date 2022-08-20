/**
 * @file hw_device.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <platform_common.h>
#include <gpu_hw_device.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}
#include <display_base.h>

#include <windows_helper.h>

namespace gpu
{    
    void d3d11_device_init_view_port(GpuDevice* self,
                                     float width, 
                                     float height);
                                    

    int 
    hw_device_set_frame(platf::Device* self) 
    {
        GpuDevice* hw = (GpuDevice*)self;
        platf::Display* disp = (platf::Display*)hw->img.display;
        d3d11::Device device = hw->device;

        // calculate convert
        hw->outY_view  = D3D11_VIEWPORT { 
            0, 0, 
            ((float)disp->width), 
            ((float)disp->height), 
            0.0f, 1.0f 
        };

        hw->outUV_view = D3D11_VIEWPORT { 
            0, 0, 
            ((float)disp->width) / 2, 
            ((float)disp->height)/ 2, 
            0.0f, 1.0f 
        };

        D3D11_TEXTURE2D_DESC t {};
        t.Width            = disp->width;
        t.Height           = disp->height;
        t.MipLevels        = 1;
        t.ArraySize        = 1;
        t.SampleDesc.Count = 1;
        t.Usage            = D3D11_USAGE_DEFAULT;
        t.Format           = hw->format;
        t.BindFlags        = D3D11_BIND_RENDER_TARGET;

        auto status = device->CreateTexture2D(&t, NULL, &hw->img.texture);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create render target texture");
            return -1;
        }

        hw->img.base.data        = (uint8*)hw->img.texture;
        hw->img.base.row_pitch   = disp->width * 4;
        hw->img.base.pixel_pitch = 4;

        int inf_size = 16 / sizeof(float);
        float info_in[inf_size] { 1.0f / (float)disp->width }; //aligned to 16-byte
        util::Buffer* buf = BUFFER_INIT((float*)info_in,16,DO_NOTHING);
        hw->info_scene = helper::convert_to_d3d11_buffer(device, buf);

        if(!info_in) {
            LOG_ERROR("Failed to create info scene buffer");
            return -1;
        }

        D3D11_RENDER_TARGET_VIEW_DESC nv12_rt_desc {
            DXGI_FORMAT_R8_UNORM,
            D3D11_RTV_DIMENSION_TEXTURE2D
        };

        status = device->CreateRenderTargetView(hw->img.texture, &nv12_rt_desc, &hw->nv12_Y_rt);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create render target view");
            return -1;
        }

        nv12_rt_desc.Format = DXGI_FORMAT_R8G8_UNORM;
        status = device->CreateRenderTargetView(hw->img.texture, &nv12_rt_desc, &hw->nv12_UV_rt);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create render target view");
            return -1;
        }

        return 0;
    }
    
    void
    hw_device_finalize( platf::Device* device)
    {
        GpuDevice* gpu = (GpuDevice*)device;
        gpu->scene_ps->Release();
        gpu->scene_vs->Release();

        gpu->convert_UV_ps->Release();
        gpu->convert_UV_vs->Release();
        gpu->convert_Y_ps->Release();

        gpu->info_scene->Release();
        gpu->input_layout->Release();

        gpu->color_matrix->Release();

        gpu->nv12_UV_rt->Release();
        gpu->nv12_Y_rt->Release();

        gpu->img.texture->Release();
        gpu->img.input_res->Release();
        gpu->img.scene_rt->Release();

        gpu->back_img.texture->Release();
        gpu->back_img.input_res->Release();
        gpu->back_img.scene_rt->Release();

        gpu->device_ctx->Release();
        free((pointer)device);
    }

    /**
     * @brief 
     * 
     * @param display 
     * @param device 
     * @param device_ctx 
     * @param pix_fmt 
     * @return int 
     */
    platf::Device*
    hw_device_init(platf::Display* display, 
                   d3d11::Device device, 
                   d3d11::DeviceContext device_ctx,
                   platf::PixelFormat pix_fmt) 
    {
        GpuDevice* self = (GpuDevice*)malloc(sizeof(GpuDevice));
        platf::Device* dev = (platf::Device*)self;
        memset((pointer)self,0,sizeof(GpuDevice));

        dev->klass = (platf::DeviceClass*)d3d11_device_class_init();
        helper::HLSL* hlsl = helper::init_hlsl();
        HRESULT status;

        device->AddRef();
        self->device_ctx = device_ctx;
        self->device = device;

        self->format = (pix_fmt == platf::PixelFormat::nv12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_P010);
        status = device->CreateVertexShader(hlsl->scene_vs_hlsl->GetBufferPointer(), hlsl->scene_vs_hlsl->GetBufferSize(), nullptr, &self->scene_vs);
        if(status) {
            LOG_ERROR("Failed to create scene vertex shader");
            return NULL;
        }

        status = device->CreatePixelShader(hlsl->convert_Y_ps_hlsl->GetBufferPointer(), hlsl->convert_Y_ps_hlsl->GetBufferSize(), nullptr, &self->convert_Y_ps);
        if(status) {
            LOG_ERROR("Failed to create convertY pixel shader");
            return NULL;
        }

        status = device->CreatePixelShader(hlsl->convert_UV_ps_hlsl->GetBufferPointer(), hlsl->convert_UV_ps_hlsl->GetBufferSize(), nullptr, &self->convert_UV_ps);
        if(status) {
            LOG_ERROR("Failed to create convertUV pixel shader");
            return NULL;
        }

        status = device->CreateVertexShader(hlsl->convert_UV_vs_hlsl->GetBufferPointer(), hlsl->convert_UV_vs_hlsl->GetBufferSize(), nullptr, &self->convert_UV_vs);
        if(status) {
            LOG_ERROR("Failed to create convertUV vertex shader");
            return NULL;
        }

        status = device->CreatePixelShader(hlsl->scene_ps_hlsl->GetBufferPointer(), hlsl->scene_ps_hlsl->GetBufferSize(), nullptr, &self->scene_ps);
        if(status) {
            LOG_ERROR("Failed to create scene pixel shader");
            return NULL;
        }


        platf::Color* color = platf::get_color();
        util::Buffer* buf = BUFFER_INIT(color,sizeof(platf::Color[4]),DO_NOTHING);
        self->color_matrix = helper::convert_to_d3d11_buffer(device, buf);
        if(!self->color_matrix) {
            LOG_ERROR("Failed to create color matrix buffer");
            return NULL;
        }

        D3D11_INPUT_ELEMENT_DESC layout_desc {
            "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
        };

        status = device->CreateInputLayout(
            &layout_desc, 1,
            hlsl->convert_UV_vs_hlsl->GetBufferPointer(), 
            hlsl->convert_UV_vs_hlsl->GetBufferSize(),
            &self->input_layout);

        self->img.display = display;

        // Color the background black, so that the padding for keeping the aspect ratio
        error::Error err = display->klass->dummy_img(display,&self->back_img.base);
        if(FILTER_ERROR(err)) {
            LOG_WARNING("Couldn't create an image to set background color to black");
            return NULL;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC desc {
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D3D11_SRV_DIMENSION_TEXTURE2D
        };
        desc.Texture2D.MipLevels = 1;

        status = device->CreateShaderResourceView(self->back_img.texture, &desc, &self->back_img.input_res);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create input shader resource view");
            return NULL;
        }

        self->device_ctx->IASetInputLayout(self->input_layout);
        self->device_ctx->PSSetConstantBuffers(0, 1, &self->color_matrix);
        self->device_ctx->VSSetConstantBuffers(0, 1, &self->info_scene);

        hw_device_set_frame(dev);
        return dev;
    }

    void 
    hw_device_set_colorspace(platf::Device* dev,
                             uint32 colorspace, 
                             uint32 color_range) 
    {
        GpuDevice* self = (GpuDevice*)dev;
        platf::Color* colors = platf::get_color();
        switch(colorspace) {
        case 5: // SWS_CS_SMPTE170M
            self->color_p = colors;
            break;
        case 1: // SWS_CS_ITU709
            self->color_p = colors + 2;
            break;
        case 9: // SWS_CS_BT2020
        default:
            LOG_WARNING("Colorspace: not yet supported: switching to default");
            self->color_p = colors;
        };

        if(color_range > 1) {
            // Full range
            ++self->color_p;
        }


        util::Buffer* buf = BUFFER_INIT(colors,sizeof(platf::Color[4]),DO_NOTHING);
        d3d11::Buffer color_matrix = (d3d11::Buffer)helper::convert_to_d3d11_buffer(self->device, buf);
        if(!color_matrix) {
            LOG_WARNING("Failed to create color matrix");
            return;
        }

        self->device_ctx->PSSetConstantBuffers(0, 1, &color_matrix);
        self->color_matrix = color_matrix;
    }

    /**
     * @brief 
     * 
     * @param img_base 
     * @return int 
     */
    error::Error
    hw_device_convert(platf::Device* dev,
                      platf::Image* img_base,
                      libav::Frame* frame) 
    {
        GpuDevice* self = (GpuDevice*)dev;
        ImageGpu* img = (ImageGpu*)img_base;
        platf::Display* disp = (platf::Display*)self->img.display;

        self->device_ctx->IASetInputLayout(self->input_layout);

        d3d11_device_init_view_port(self,disp->width, disp->height);
        self->device_ctx->OMSetRenderTargets(1, &self->nv12_Y_rt, nullptr);
        self->device_ctx->VSSetShader(self->scene_vs, nullptr, 0);
        self->device_ctx->PSSetShader(self->convert_Y_ps, nullptr, 0);
        self->device_ctx->PSSetShaderResources(0, 1, &self->back_img.input_res);
        self->device_ctx->Draw(3, 0);

        self->device_ctx->RSSetViewports(1, &self->outY_view);
        self->device_ctx->PSSetShaderResources(0, 1, &img->input_res);
        self->device_ctx->Draw(3, 0);
        self->device_ctx->Flush();
        // Artifacts start appearing on the rendered image if Sunshine doesn't flush
        // before rendering on the UV part of the image.

        d3d11_device_init_view_port(self,disp->width / 2, disp->height / 2);
        self->device_ctx->OMSetRenderTargets(1, &self->nv12_UV_rt, nullptr);
        self->device_ctx->VSSetShader(self->convert_UV_vs, nullptr, 0);
        self->device_ctx->PSSetShader(self->convert_UV_ps, nullptr, 0);  
        self->device_ctx->PSSetShaderResources(0, 1, &self->back_img.input_res);
        self->device_ctx->Draw(3, 0);

        self->device_ctx->RSSetViewports(1, &self->outUV_view);
        self->device_ctx->PSSetShaderResources(0, 1, &img->input_res);
        self->device_ctx->Draw(3, 0);
        self->device_ctx->Flush();

        if(!frame->buf[0]) 
            frame->buf[0] = av_buffer_allocz(sizeof(AVD3D11FrameDescriptor));

        AVD3D11FrameDescriptor* desc     = (AVD3D11FrameDescriptor *)frame->buf[0]->data;
        desc->texture = (d3d11::Texture2D)self->img.base.data;
        desc->index   = 0;

        frame->data[0] = self->img.base.data;
        frame->data[1] = 0;

        frame->linesize[0] = self->img.base.row_pitch;

        frame->height = disp->height;
        frame->width  = disp->width;
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        frame->key_frame = 0;

        return error::ERROR_NONE;
    }

    void 
    _init_view_port(GpuDevice* self,
                    float x, 
                    float y, 
                    float width, 
                    float height) 
    {
        D3D11_VIEWPORT view {
            x, y,
            width, height,
            0.0f, 1.0f
        };

        self->device_ctx->RSSetViewports(1, &view);
    }

    void 
    d3d11_device_init_view_port(GpuDevice* self,
                                float width, 
                                float height) 
    {
        _init_view_port((GpuDevice*)self,0.0f, 0.0f, width, height);
    }

    GpuDeviceClass*       
    d3d11_device_class_init()
    {
        static GpuDeviceClass klass;
        RETURN_PTR_ONCE(klass);
        
        klass.base.convert            = hw_device_convert;
        klass.base.init               = hw_device_init;
        klass.base.finalize           = hw_device_finalize;
        klass.base.set_colorspace     = hw_device_set_colorspace;
        klass.init_view_port          = d3d11_device_init_view_port;
        return &klass;
    }
    
} // namespace gpu


