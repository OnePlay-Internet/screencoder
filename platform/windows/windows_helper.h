/**
 * @file windows_helper.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __WINDOWS_HELPER_H__
#define __WINDOWS_HELPER_H__

#include <d3d11_datatype.h>
#include <sunshine_util.h>

namespace helper
{
    directx::d3d11::Buffer convert_to_d3d11_buffer          (directx::d3d11::Device device, 
                                                 util::Buffer* t);

    directx::d3d11::BlendState make_blend       (directx::d3d11::Device device, 
                                                 bool enable);

    util::Buffer*           make_cursor_image   (util::Buffer* img_data, 
                                                 DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info);
        


    int                     init_render_target_b(directx::d3d11::Device device, 
                                                 directx::d3d11::ShaderResourceView shader_res, 
                                                 directx::d3d11::RenderTargetView render_target, 
                                                 int width, int height, 
                                                 DXGI_FORMAT format);


    /**
     * @brief 
     * 
     * @param file 
     * @return directx::d3d::Blob 
     */
    directx::d3d::Blob      compile_vertex_shader(LPCSTR file);

    /**
     * @brief 
     * 
     * @param file 
     * @return directx::d3d::Blob 
     */
    directx::d3d::Blob      compile_pixel_shader(LPCSTR file);
} // namespace helper



#endif