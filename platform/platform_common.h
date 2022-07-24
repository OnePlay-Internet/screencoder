//
// Created by loki on 6/21/19.
//

#ifndef SUNSHINE_COMMON_H
#define SUNSHINE_COMMON_H
#include <sunshine_util.h>

#include <bitset>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <d3d11_datatype.h>


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
        reinit,
        timeout,
        error
    }Capture;

    typedef struct _Image {
        byte* data;
        int32 width;
        int32 height;
        int32 pixel_pitch;
        int32 row_pitch;
    }Image;

    typedef struct _HWDeviceClass DeviceClass;

    typedef struct _DisplayClass DisplayClass;

    typedef struct _Display      Display;

    typedef struct _HWDevice {
        DeviceClass* klass;
        void *data;
        libav::Frame* frame;
    }Device;

    struct _HWDeviceClass {
        Device* (*init)       (platf::Display* display, 
                                 d3d11::Device device_p, 
                                 d3d11::DeviceContext device_ctx_p,
                                 platf::PixelFormat pix_fmt);

        int (*convert)          (Device* self,
                                 Image* img);

        void (*finalize)         (Device* self);
        /**
         * @brief 
         * do the conversion from libav::Frame to ImageD3D
         */
        int (*set_frame)        (Device* self,
                                 libav::Frame* frame);

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
    typedef Capture (*SnapshootCallback) (Image* img,
                                          util::Buffer* data,
                                          encoder::EncodeThreadContext* thread_ctx);


    struct _Display {
        DisplayClass* klass;

        // Offsets for when streaming a specific monitor. By default, they are 0.
        int offset_x, offset_y;
        int env_width, env_height;
        int width, height;
    };

    struct _DisplayClass {
        Display*    (*init)             (int framerate, 
                                         char* display_name);

        void        (*finalize)         (void* self);

        int         (*dummy_img)        (Display* self,
                                         Image* img);

        Image*      (*alloc_img)        (Display* self);

        Device*     (*make_hwdevice)    (Display* self,
                                         PixelFormat pix_fmt);
        
        Capture     (*capture)          (Display* self,
                                         Image* img, 
                                         SnapshootCallback snapshot_cb, 
                                         util::Buffer* data,
                                         encoder::EncodeThreadContext* thread_ctx,
                                         bool cursor);

        Capture     (*snapshot)         (Display* self,
                                         platf::Image *img_base, 
                                         std::chrono::milliseconds timeout, 
                                         bool cursor_visible); 
    };


    /**
     * display_name --> The name of the monitor that SHOULD be displayed
     *    If display_name is empty --> Use the first monitor that's compatible you can find
     *    If you require to use this parameter in a seperate thread --> make a copy of it.
     * 
     * framerate --> The peak number of images per second
     * 
     * Returns Display based on hwdevice_type
     */
    Display*                get_display       (MemoryType hwdevice_type , 
                                          char* display_name , 
                                          int framerate);


    // A list of names of displays accepted as display_name with the MemoryType
    char**                  display_names (MemoryType hwdevice_type);

    /**
     * @brief Get the color object
     * 
     * @return Color* 
     */
    Color*                  get_color     ();


    Display*                tryget_display(libav::HWDeviceType type, 
                                           char* display_name, 
                                           int framerate);

    PixelFormat             map_pix_fmt     (libav::PixelFormat fmt);
} // namespace platf

#endif //SUNSHINE_COMMON_H
