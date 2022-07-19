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
#include <encoder_packet.h>


namespace platf {
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

    typedef struct _HWDeviceClass HWDeviceClass;

    typedef struct _DisplayClass DisplayClass;

    typedef struct _Display      Display;

    typedef struct _HWDevice {
        HWDeviceClass* klass;
        void *data;
        libav::Frame* frame;
    }HWDevice;

    struct _HWDeviceClass {
        HWDevice* (*init)       (platf::Display* display, 
                                 directx::d3d11::Device device_p, 
                                 directx::d3d11::DeviceContext device_ctx_p,
                                 platf::PixelFormat pix_fmt);

        int (*convert)          (HWDevice* self,
                                 Image* img);

        int (*finalize)         (HWDevice* self);
        /**
         * @brief 
         * do the conversion from ImageD3D to libav::Frame
         */
        int (*set_frame)        (HWDevice* self,
                                 libav::Frame* frame);

        void (*set_colorspace)  (HWDevice* self,
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
                                       util::ListObject* synced_sessions,
                                       util::ListObject* synced_session_ctxs,
                                       util::QueueArray* encode_session_ctx_queue);


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

        void        (*free)             (Display* self);

        HWDevice*   (*make_hwdevice)    (Display* self,
                                         PixelFormat pix_fmt);
        
        Capture     (*capture)          (Display* self,
                                         Image* img, 
                                         SnapshootCallback snapshot_cb, 
                                         util::ListObject* synced_sessions,
                                         util::ListObject* synced_session_ctxs,
                                         util::QueueArray* encode_session_ctx_queue,
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
    Display*                display       (MemoryType hwdevice_type , 
                                          char* display_name , 
                                          int framerate);


    // A list of names of displays accepted as display_name with the MemoryType
    char**                  display_names (MemoryType hwdevice_type);

    /**
     * @brief Get the color object
     * 
     * @return Color* 
     */
    encoder::Color*                  get_color     ();
} // namespace platf

#endif //SUNSHINE_COMMON_H
