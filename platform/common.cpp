/**
 * @file common.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <common.h>
#include <encoder_packet.h>
#include <display_vram.h>





namespace platf
{
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
    

    encoder::Color 
    make_color_matrix(float Cr, float Cb, 
                      float U_max, float V_max, 
                      float add_Y, float add_UV, 
                      const float2 &range_Y, const float2 &range_UV) 
    {
        float Cg = 1.0f - Cr - Cb;

        float Cr_i = 1.0f - Cr;
        float Cb_i = 1.0f - Cb;

        float shift_y  = range_Y[0] / 256.0f;
        float shift_uv = range_UV[0] / 256.0f;

        float scale_y  = (range_Y[1] - range_Y[0]) / 256.0f;
        float scale_uv = (range_UV[1] - range_UV[0]) / 256.0f;
        return {
            { Cr, Cg, Cb, add_Y },
            { -(Cr * U_max / Cb_i), -(Cg * U_max / Cb_i), U_max, add_UV },
            { V_max, -(Cg * V_max / Cr_i), -(Cb * V_max / Cr_i), add_UV },
            { scale_y, shift_y },
            { scale_uv, shift_uv },
        };
    }

    encoder::Color*
    get_color()
    {
        static bool init = false;
        static Color colors[4];
        if(init)
            return &colors;

        init = true; 
        colors = 
        {
            make_color_matrix(0.299f, 0.114f, 0.436f, 0.615f, 0.0625, 0.5f, { 16.0f, 235.0f }, { 16.0f, 240.0f }),   // BT601 MPEG
            make_color_matrix(0.299f, 0.114f, 0.5f, 0.5f, 0.0f, 0.5f, { 0.0f, 255.0f }, { 0.0f, 255.0f }),           // BT601 JPEG
            make_color_matrix(0.2126f, 0.0722f, 0.436f, 0.615f, 0.0625, 0.5f, { 16.0f, 235.0f }, { 16.0f, 240.0f }), // BT701 MPEG
            make_color_matrix(0.2126f, 0.0722f, 0.5f, 0.5f, 0.0f, 0.5f, { 0.0f, 255.0f }, { 0.0f, 255.0f }),         // BT701 JPEG
        };
        return &colors;
    }
} // namespace platf