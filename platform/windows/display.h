//
// Created by loki on 4/23/20.
//

#ifndef SUNSHINE_DISPLAY_H
#define SUNSHINE_DISPLAY_H

#include <d3d11.h>
#include <d3d11_4.h>
#include <d3dcommon.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <sunshine_util.h>
#include <utility.h>

#include <d3d11_datatype.h>
#include <common.h>
#include <duplication.h>



namespace display {
    typedef struct _HLSL
    {  
      directx::d3d::Blob convert_UV_vs_hlsl;
      directx::d3d::Blob convert_UV_ps_hlsl;
      directx::d3d::Blob scene_vs_hlsl;
      directx::d3d::Blob convert_Y_ps_hlsl;
      directx::d3d::Blob scene_ps_hlsl;
    }HLSL;


    typedef struct _Cursor {
        byte* img_data;
        DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info;
        int x, y;
        bool visible;
    }Cursor;

    typedef struct _GpuCursor {
        directx::d3d11::Texture2D texture;
        directx::d3d11::ShaderResourceView input_res;
        D3D11_VIEWPORT cursor_view;
        bool visible;
    }GpuCursor;



    typedef enum _D3DKMT_SCHEDULINGPRIORITYCLASS {
        D3DKMT_SCHEDULINGPRIORITYCLASS_IDLE,
        D3DKMT_SCHEDULINGPRIORITYCLASS_BELOW_NORMAL,
        D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL,
        D3DKMT_SCHEDULINGPRIORITYCLASS_ABOVE_NORMAL,
        D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH,
        D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME
    } D3DKMT_SCHEDULINGPRIORITYCLASS;

    typedef NTSTATUS WINAPI (*PD3DKMTSetProcessSchedulingPriorityClass)(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS);

    typedef struct _DisplayBase {
        platf::Display base;

        directx::dxgi::Factory factory;
        directx::dxgi::Adapter adapter;
        directx::dxgi::Output  output;
        directx::d3d11::Device device;
        directx::d3d11::DeviceContext device_ctx;

        duplication::Duplication dup;

        std::chrono::nanoseconds delay;
        DXGI_FORMAT format;
        D3D_FEATURE_LEVEL feature_level;
    }DisplayBase;

    typedef struct _DisplayRam{
      DisplayBase base;
      Cursor cursor;

      D3D11_MAPPED_SUBRESOURCE img_info;
      directx::d3d11::Texture2D texture;
    }DisplayRam;

    /**
     * @brief 
     * 
     * @param framerate 
     * @param display_name 
     * @return int 
     */
    int             display_base_init           (DisplayBase* self,
                                                 int framerate, 
                                                 const char* display_name);

} // namespace platf::dxgi

#endif
