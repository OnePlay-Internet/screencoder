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
#include <screencoder_util.h>
#include <gpu_hw_device.h>

namespace helper
{

    d3d11::Buffer convert_to_d3d11_buffer          (d3d11::Device device, 
                                                 util::Buffer* t);

    d3d11::BlendState make_blend       (d3d11::Device device, 
                                                 bool enable);

    uint8*           make_cursor_image   (uint8* buffer, 
                                          int buffer_size,
                                          DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info,
                                          int* outsize);
        






    /**
     * @brief 
     * 
     * @param file 
     * @return d3d::Blob 
     */
    d3d::Blob      compile_vertex_shader(LPCSTR file);

    /**
     * @brief 
     * 
     * @param file 
     * @return d3d::Blob 
     */
    d3d::Blob      compile_pixel_shader(LPCSTR file);


    /**
     * @brief 
     * 
     * @param type 
     * @return platf::MemoryType 
     */
    platf::MemoryType       map_dev_type        (libav::HWDeviceType type) ;

    typedef struct _HLSL
    {  
      d3d::Blob convert_UV_vs_hlsl;
      d3d::Blob convert_UV_ps_hlsl;
      d3d::Blob scene_vs_hlsl;
      d3d::Blob convert_Y_ps_hlsl;
      d3d::Blob scene_ps_hlsl;
    }HLSL;

    /**
     * @brief 
     * 
     * @return HLSL* 
     */
    HLSL*          init_hlsl                    (); 
} // namespace helper



#endif