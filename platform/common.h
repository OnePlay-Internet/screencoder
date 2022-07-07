//
// Created by loki on 6/21/19.
//

#ifndef SUNSHINE_COMMON_H
#define SUNSHINE_COMMON_H

#include <bitset>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <sunshine_datatype.h>
#include <avcodec_datatype.h>

#include <utility.h>

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
        unknown
    }PixelFormat;

    typedef enum _Capture{
        ok,
        reinit,
        timeout,
        error
    }Capture;

    typedef struct _Image {
        uint8 *data;
        int32 width;
        int32 height;
        int32 pixel_pitch;
        int32 row_pitch;
    }Image;


    typedef struct _HWDevice {
        void *data;
        libav::Frame* frame;
    }HWDevice;


    typedef void (*SnapshootCallback) (Image* img);


    typedef struct _Display {
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
        SnapshootCallback callback;

        // Offsets for when streaming a specific monitor. By default, they are 0.
        int offset_x, offset_y;
        int env_width, env_height;
        int width, height;
    }Display;


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
                                          const std::string &display_name , 
                                          int framerate);


    // A list of names of displays accepted as display_name with the MemoryType
    char**                  display_names (MemoryType hwdevice_type);
} // namespace platf

#endif //SUNSHINE_COMMON_H
