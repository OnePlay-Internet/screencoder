/**
 * @file platform_common.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-09-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef SUNSHINE_COMMON_H
#define SUNSHINE_COMMON_H
#include <screencoder_util.h>

#include <bitset>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <d3d11_datatype.h>

#include <mutex>


using float4 = float[4];
using float3 = float[3];
using float2 = float[2];



namespace platf {    
    
    
    typedef struct _Color {
        float4 color_vec_y;
        float4 color_vec_u;
        float4 color_vec_v;
        float2 range_y;
        float2 range_uv;
    }Color;


    typedef enum _MemoryType {
        system,
        vaapi,
        dxgi,
        cuda,
        unknown
    }MemoryType ;

    typedef enum _PixelFormat {
        yuv420p,
        yuv420p10,
        nv12,
        p010,
        unknown_pixelformat
    }PixelFormat;

    typedef enum _Capture{
        ok,
        timeout,
        error,
        reinit
    }Capture;

    typedef struct _Image {
        uint8* data;
        int32 pixel_pitch;
        int32 row_pitch;
    }Image;

    typedef struct _Cursor{
    }Cursor;

    typedef struct _HWDeviceClass DeviceClass;

    typedef struct _DisplayClass DisplayClass;

    typedef struct _Display      Display;

    typedef struct _HWDevice {
        DeviceClass* klass;
    }Device;

    struct _HWDeviceClass {
        Device* (*init)       (platf::Display* display, 
                                 d3d11::Device device_p, 
                                 d3d11::DeviceContext device_ctx_p,
                                 platf::PixelFormat pix_fmt);

        error::Error (*convert) (Device* self,
                                 Image* img,
                                 libav::Frame* frame);

        /**
         * @brief 
         * allocate avcodec and d3d11 texture resources
         * call once during make_encode_context_buffer
         */
        void  (*finalize)        (Device* self);

        void (*set_colorspace)  (Device* self,
                                 uint32 colorspace, 
                                 uint32 color_range);
    };


    /**
     * When display has a new image ready, this callback will be called with the new image.
     * 
     * On Break Request -->
     *    Returns nullptr
     * 
     * On Success -->
     *    Returns the image object that should be filled next.
     *    This may or may not be the image send with the callback
     */
    typedef Capture (*SnapshootCallback) (Image** img,
                                          util::Buffer* data,
                                          encoder::EncodeThreadContext* thread_ctx);


    struct _Display {
        DisplayClass* klass;
        char name[100];

        // Offsets for when streaming a specific monitor. By default, they are 0.
        int offset_x, offset_y;
        int env_width, env_height;
        int width, height;
        int framerate;
    };

    struct _DisplayClass {
        Display*               (*init)             (char* display_name);

        error::Error           (*dummy_img)        (Display* self,
                                                     Image* img);

        util::Buffer*          (*alloc_img)        (Display* self);

        void                   (*free_resources)             (Display* self);

        Device*                (*make_hwdevice)    (Display* self,
                                                    PixelFormat pix_fmt);
        
        Capture                (*capture)          (Display* self,
                                                    Image* img);

        util::Buffer*          (*allocate_cursor)  (Display* self,
                                                    Image* img);

        Capture                (*draw_cursor)       (Display* self,
                                                    Image* img,
                                                    Cursor* cursor);
    };


    /**
     * display_name --> The name of the monitor that SHOULD be displayed
     * 
     * Returns Display based on hwdevice_type
     */
    Display*                get_display_by_name       (MemoryType hwdevice_type , 
                                                       char* display_name);


    // A list of names of displays accepted as display_name with the MemoryType
    char**                  display_names (MemoryType hwdevice_type);

    /**
     * @brief Get the color object
     * 
     * @return Color* 
     */
    Color*                  get_color     ();


    PixelFormat             map_pix_fmt     (libav::PixelFormat fmt);
} // namespace platf

#endif //SUNSHINE_COMMON_H
