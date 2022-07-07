

#include <common.h>
#include <hw_device.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}

#include <windows_helper.h>

namespace hwdevice
{
    int 
    hw_device_set_frame(D3D11Device* hw,
                        libav::Frame* frame) 
    {
      if(hw->hwframe)
        av_frame_free(hw->hwframe);

      hw->hwframe    = frame;
      hw->base.frame = frame;

      directx::d3d11::Device device = (directx::d3d11::Device)hw->base.data;

      int out_width  = frame->width;
      int out_height = frame->height;

      float in_width  = img.display->width;
      float in_height = img.display->height;

      // // Ensure aspect ratio is maintained
      auto scalar       = std::fminf(out_width / in_width, out_height / in_height);
      auto out_width_f  = in_width * scalar;
      auto out_height_f = in_height * scalar;

      // result is always positive
      auto offsetX = (out_width - out_width_f) / 2;
      auto offsetY = (out_height - out_height_f) / 2;

      hw->outY_view  = D3D11_VIEWPORT 
      { 
        offsetX, 
        offsetY, 
        out_width_f, 
        out_height_f, 
        0.0f, 1.0f 
      };

      hw->outUV_view = D3D11_VIEWPORT 
      { 
        offsetX / 2, 
        offsetY / 2, 
        out_width_f / 2, 
        out_height_f / 2, 
        0.0f, 1.0f 
      };

      D3D11_TEXTURE2D_DESC t {};
      t.Width            = out_width;
      t.Height           = out_height;
      t.MipLevels        = 1;
      t.ArraySize        = 1;
      t.SampleDesc.Count = 1;
      t.Usage            = D3D11_USAGE_DEFAULT;
      t.Format           = hw->format;
      t.BindFlags        = D3D11_BIND_RENDER_TARGET;

      auto status = device->CreateTexture2D(&t, NULL, &img.texture);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create render target texture [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      img.width       = out_width;
      img.height      = out_height;
      img.data        = (std::uint8_t *)img.texture.get();
      img.row_pitch   = out_width * 4;
      img.pixel_pitch = 4;

      float info_in[16 / sizeof(float)] { 1.0f / (float)out_width }; //aligned to 16-byte
      hw->info_scene = helper::make_buffer(device, info_in);

      if(!info_in) {
        // BOOST_LOG(error) << "Failed to create info scene buffer"sv;
        return -1;
      }

      D3D11_RENDER_TARGET_VIEW_DESC nv12_rt_desc {
        DXGI_FORMAT_R8_UNORM,
        D3D11_RTV_DIMENSION_TEXTURE2D
      };

      status = device_p->CreateRenderTargetView(img.texture.get(), &nv12_rt_desc, &hw->nv12_Y_rt);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create render target view [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      nv12_rt_desc.Format = DXGI_FORMAT_R8G8_UNORM;
      status = device_p->CreateRenderTargetView(img.texture.get(), &nv12_rt_desc, &hw->nv12_UV_rt);
      if(FAILED(status)) {
        // BOOST_LOG(error) << "Failed to create render target view [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      // Need to have something refcounted
      if(!frame->buf[0]) {
        frame->buf[0] = av_buffer_allocz(sizeof(AVD3D11FrameDescriptor));
      }

      auto desc     = (AVD3D11FrameDescriptor *)frame->buf[0]->data;
      desc->texture = (ID3D11Texture2D *)img.data;
      desc->index   = 0;

      frame->data[0] = img.data;
      frame->data[1] = 0;

      frame->linesize[0] = img.row_pitch;

      frame->height = img.height;
      frame->width  = img.width;

      return 0;
    }

    /**
     * @brief 
     * 
     * @param display 
     * @param device_p 
     * @param device_ctx_p 
     * @param pix_fmt 
     * @return int 
     */
    int 
    hw_device_init( D3D11Device* self,
                platf::Display* display, 
                directx::d3d11::Device device_p, 
                directx::d3d11::DeviceContext device_ctx_p,
                platf::PixelFormat pix_fmt) 
    {
      HLSL* hlsl = init_hlsl();
      HRESULT status;

      device_p->AddRef();
      self->base->data = device_p;

      self->device_ctx = device_ctx_p;

      self->format = (pix_fmt == platf::PixelFormat::nv12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_P010);
      status = device_p->CreateVertexShader(hlsl->scene_vs_hlsl->GetBufferPointer(), hlsl->scene_vs_hlsl->GetBufferSize(), nullptr, &sefl->scene_vs);
      if(status) {
        // BOOST_LOG(error) << "Failed to create scene vertex shader [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      status = device_p->CreatePixelShader(hlsl->convert_Y_ps_hlsl->GetBufferPointer(), hlsl->convert_Y_ps_hlsl->GetBufferSize(), nullptr, &sef->convert_Y_ps);
      if(status) {
        // BOOST_LOG(error) << "Failed to create convertY pixel shader [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      status = device_p->CreatePixelShader(hlsl->convert_UV_ps_hlsl->GetBufferPointer(), hlsl->convert_UV_ps_hlsl->GetBufferSize(), nullptr, &sefl->convert_UV_ps);
      if(status) {
        // BOOST_LOG(error) << "Failed to create convertUV pixel shader [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      status = device_p->CreateVertexShader(hlsl->convert_UV_vs_hlsl->GetBufferPointer(), hlsl->convert_UV_vs_hlsl->GetBufferSize(), nullptr, &self->convert_UV_vs);
      if(status) {
        // BOOST_LOG(error) << "Failed to create convertUV vertex shader [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      status = device_p->CreatePixelShader(scene_ps_hlsl->GetBufferPointer(), scene_ps_hlsl->GetBufferSize(), nullptr, &scene_ps);
      if(status) {
        // BOOST_LOG(error) << "Failed to create scene pixel shader [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }


      self->color_matrix = helper::make_buffer(device_p, *(encoder::get_color() + 0));
      if(!self->color_matrix) {
        // BOOST_LOG(error) << "Failed to create color matrix buffer"sv;
        return -1;
      }

      D3D11_INPUT_ELEMENT_DESC layout_desc {
        "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
      };

      status = device_p->CreateInputLayout(
        &layout_desc, 1,
        convert_UV_vs_hlsl->GetBufferPointer(), convert_UV_vs_hlsl->GetBufferSize(),
        &sefl->input_layout);

      img.display = std::move(display);

      // Color the background black, so that the padding for keeping the aspect ratio
      // is black
      if(img.display->dummy_img(&self->back_img)) {
        // BOOST_LOG(warning) << "Couldn't create an image to set background color to black"sv;
        return -1;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC desc {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D3D11_SRV_DIMENSION_TEXTURE2D
      };
      desc.Texture2D.MipLevels = 1;

      status = device_p->CreateShaderResourceView(back_img.texture.get(), &desc, &back_img.input_res);
      if(FAILED(status)) {
        BOOST_LOG(error) << "Failed to create input shader resource view [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      device_ctx_p->IASetInputLayout(input_layout.get());
      device_ctx_p->PSSetConstantBuffers(0, 1, &self->color_matrix);
      device_ctx_p->VSSetConstantBuffers(0, 1, &self->info_scene);

      return 0;
    }

    void 
    hw_device_set_colorspace(D3D11Device* self,
                             uint32 colorspace, 
                             uint32 color_range) 
    {
      encoder::Color* colors = encoder::get_color();
      switch(colorspace) {
      case 5: // SWS_CS_SMPTE170M
        self->color_p = colors;
        break;
      case 1: // SWS_CS_ITU709
        self->color_p = colors + 2;
        break;
      case 9: // SWS_CS_BT2020
      default:
        // BOOST_LOG(warning) << "Colorspace: ["sv << colorspace << "] not yet supported: switching to default"sv;
        self->color_p = colors;
      };

      if(color_range > 1) {
        // Full range
        ++self->color_p;
      }

      byte* color_matrix = helper::make_buffer((directx::d3d11::Device*)self->base.data, *self->color_p);
      if(!color_matrix) {
        // BOOST_LOG(warning) << "Failed to create color matrix"sv;
        return;
      }

      device_ctx_p->PSSetConstantBuffers(0, 1, &color_matrix);
      self->color_matrix = std::move(color_matrix);
    }

    /**
     * @brief 
     * 
     * @param img_base 
     * @return int 
     */
    int 
    hw_device_convert(D3D11Device* self,
                      platf::Image img_base) 
    {
        ImageD3D img = (ImageD3D)img_base;

        device_ctx_p->IASetInputLayout(input_layout.get());

        _init_view_port(self->img.width, self->img.height);
        device_ctx_p->OMSetRenderTargets(1, &self->nv12_Y_rt, nullptr);
        device_ctx_p->VSSetShader(scene_vs.get(), nullptr, 0);
        device_ctx_p->PSSetShader(convert_Y_ps.get(), nullptr, 0);
        device_ctx_p->PSSetShaderResources(0, 1, &back_img.input_res);
        device_ctx_p->Draw(3, 0);

        device_ctx_p->RSSetViewports(1, &self->outY_view);
        device_ctx_p->PSSetShaderResources(0, 1, &img.input_res);
        device_ctx_p->Draw(3, 0);

        // Artifacts start appearing on the rendered image if Sunshine doesn't flush
        // before rendering on the UV part of the image.
        device_ctx_p->Flush();

        _init_view_port(self->img.width / 2, self->img.height / 2);
        device_ctx_p->OMSetRenderTargets(1, &self->nv12_UV_rt, nullptr);
        device_ctx_p->VSSetShader(convert_UV_vs.get(), nullptr, 0);
        device_ctx_p->PSSetShader(convert_UV_ps.get(), nullptr, 0);
        device_ctx_p->PSSetShaderResources(0, 1, &back_img.input_res);
        device_ctx_p->Draw(3, 0);

        device_ctx_p->RSSetViewports(1, &self->outUV_view);
        device_ctx_p->PSSetShaderResources(0, 1, &img.input_res);
        device_ctx_p->Draw(3, 0);
        device_ctx_p->Flush();

        return 0;
    }
    
} // namespace hwdevice


