//
// Created by loki on 4/23/20.
//

#ifndef SUNSHINE_DISPLAY_H
#define SUNSHINE_DISPLAY_H
#include <screencoder_util.h>

#include <d3d11.h>
#include <d3d11_4.h>
#include <d3dcommon.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>


#include <d3d11_datatype.h>
#include <platform_common.h>
#include <duplication.h>



namespace display {
    typedef struct _DisplayBase {
        platf::Display base;

        dxgi::Factory factory;
        dxgi::Adapter adapter;
        dxgi::Output  output;
        d3d11::Device device;
        d3d11::DeviceContext device_ctx;

        duplication::Duplication dup;

        std::chrono::nanoseconds delay;
        DXGI_FORMAT format;
        D3D_FEATURE_LEVEL feature_level;
    }DisplayBase;




    /**
     * @brief 
     * 
     * @param framerate 
     * @param display_name 
     * @return int 
     */
    int             display_base_init           (DisplayBase* self,
                                                 int framerate, 
                                                 char* display_name);

} // namespace platf::dxgi

#endif
