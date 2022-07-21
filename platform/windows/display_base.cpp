/**
 * @file display_base.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_util.h>

#include <cmath>
#include <codecvt>

#include <display_base.h>
#include <sunshine_config.h>

#include <platform_common.h>



#include <d3d11_datatype.h>
#include <windows_helper.h>
#include <thread>
#include <cstdlib>
#include <string.h>

using namespace std::literals;


// TODO: setup shader dir
#define SUNSHINE_ASSETS_DIR
#define SUNSHINE_SHADERS_DIR SUNSHINE_ASSETS_DIR "/shaders/directx"

namespace display{
  HDESK 
  syncThreadDesktop() 
  {
    auto hDesk = OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, FALSE, GENERIC_ALL);
    if(!hDesk) {
      auto err = GetLastError();
      // BOOST_LOG(error) << "Failed to Open Input Desktop [0x"sv << util::hex(err).to_string_view() << ']';
      return nullptr;
    }

    if(!SetThreadDesktop(hDesk)) {
      auto err = GetLastError();
      // BOOST_LOG(error) << "Failed to sync desktop to thread [0x"sv << util::hex(err).to_string_view() << ']';
    }

    CloseDesktop(hDesk);

    return hDesk;
  }


  bool 
  string_compare(char* a, char* b){
    int i = 0;
    if (strlen(a) != strlen(b))
      return false;
    
    while (*(a + i) == *(b + i))
    {
      i++;
    }

    if (strlen(a) == i)
      return true;  
    else
      return false;
  }

  int
  display_base_init(DisplayBase* self,
                    int framerate, 
                    char* display_name) 
  {
    // Ensure we can duplicate the current display
    syncThreadDesktop();

    self->delay = std::chrono::nanoseconds { 1s } / framerate;

    // Get rectangle of full desktop for absolute mouse coordinates
    self->base.env_width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    self->base.env_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HRESULT status;

    status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **)&self->factory);
    if(FAILED(status)) {
      LOG_ERROR("Failed to create DXGIFactory1");
      return -1;
    }

    directx::dxgi::Adapter adapter_p;
    for(int x = 0; self->factory->EnumAdapters1(x, &adapter_p) != DXGI_ERROR_NOT_FOUND; ++x) {
      DXGI_ADAPTER_DESC1 adapter_desc;
      directx::dxgi::Adapter adapter_temp = adapter_p;
      adapter_temp->GetDesc1(&adapter_desc);

      char str[100] = {0};
      wcstombs(str, adapter_desc.Description, 12);
      if(!ENCODER_CONFIG->adapter_name && string_compare(str,ENCODER_CONFIG->adapter_name)) 
      {
          continue;
      }

      directx::dxgi::Output output;
      for(int y = 0; adapter_temp->EnumOutputs(y, &output) != DXGI_ERROR_NOT_FOUND; ++y) {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        char str2[100] = {0};
        wcstombs(str2, desc.DeviceName, 12);
        if(!ENCODER_CONFIG->output_name && string_compare(str2,display_name)) {
          continue;
        }

        if(desc.AttachedToDesktop) {
          self->output = output;

          self->base.offset_x = desc.DesktopCoordinates.left;
          self->base.offset_y = desc.DesktopCoordinates.top;
          self->base.width    = desc.DesktopCoordinates.right -  self->base.offset_x;
          self->base.height   = desc.DesktopCoordinates.bottom - self->base.offset_y;

          // left and bottom may be negative, yet absolute mouse coordinates start at 0x0
          // Ensure offset starts at 0x0
          self->base.offset_x -= GetSystemMetrics(SM_XVIRTUALSCREEN);
          self->base.offset_y -= GetSystemMetrics(SM_YVIRTUALSCREEN);
        }
      }

      if(self->output) {
        self->adapter = adapter_temp;
        break;
      }
    }

    if(!self->output) {
      LOG_ERROR("Failed to locate an output device");
      return -1;
    }

    D3D_FEATURE_LEVEL featureLevels[] {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_9_3,
      D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1
    };

    status = self->adapter->QueryInterface(IID_IDXGIAdapter, (void **)&adapter_p);
    if(FAILED(status)) {
      // BOOST_LOG(error) << "Failed to query IDXGIAdapter interface"sv;
      return -1;
    }

    status = D3D11CreateDevice(
      adapter_p,
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr,
      D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
      featureLevels, 
      sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
      D3D11_SDK_VERSION,
      &self->device,
      &self->feature_level,
      &self->device_ctx);

    adapter_p->Release();

    if(FAILED(status)) {
      // BOOST_LOG(error) << "Failed to create D3D11 device [0x"sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    DXGI_ADAPTER_DESC adapter_desc;
    self->adapter->GetDesc(&adapter_desc);

    // auto description = converter.to_bytes(adapter_desc.Description);
    // BOOST_LOG(info)
    //   << std::endl
    //   << "Device Description : " << description << std::endl
    //   << "Device Vendor ID   : 0x"sv << util::hex(adapter_desc.VendorId).to_string_view() << std::endl
    //   << "Device Device ID   : 0x"sv << util::hex(adapter_desc.DeviceId).to_string_view() << std::endl
    //   << "Device Video Mem   : "sv << adapter_desc.DedicatedVideoMemory / 1048576 << " MiB"sv << std::endl
    //   << "Device Sys Mem     : "sv << adapter_desc.DedicatedSystemMemory / 1048576 << " MiB"sv << std::endl
    //   << "Share Sys Mem      : "sv << adapter_desc.SharedSystemMemory / 1048576 << " MiB"sv << std::endl
    //   << "Feature Level      : 0x"sv << util::hex(feature_level).to_string_view() << std::endl
    //   << "Capture size       : "sv << width << 'x' << height << std::endl
    //   << "Offset             : "sv << offset_x << 'x' << offset_y << std::endl
    //   << "Virtual Desktop    : "sv << env_width << 'x' << env_height;

    // Enable DwmFlush() only if the current refresh rate can match the client framerate.
    auto refresh_rate = framerate;
    DWM_TIMING_INFO timing_info;
    timing_info.cbSize = sizeof(timing_info);

    status = DwmGetCompositionTimingInfo(NULL, &timing_info);
    if(FAILED(status)) {
      LOG_ERROR("Failed to detect active refresh rate");
    } else {
      refresh_rate = std::round((double)timing_info.rateRefresh.uiNumerator / (double)timing_info.rateRefresh.uiDenominator);
    }

    self->dup.use_dwmflush = ENCODER_CONFIG->dwmflush && !(framerate > refresh_rate) ? true : false;

    // Bump up thread priority
    {
      const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
      TOKEN_PRIVILEGES tp;
      HANDLE token;
      LUID val;

      if(OpenProcessToken(GetCurrentProcess(), flags, &token) &&
        !!LookupPrivilegeValue(NULL, SE_INC_BASE_PRIORITY_NAME, &val)) {
        tp.PrivilegeCount           = 1;
        tp.Privileges[0].Luid       = val;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if(!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL, NULL)) {
          // BOOST_LOG(warning) << "Could not set privilege to increase GPU priority";
        }
      }

      CloseHandle(token);

      HMODULE gdi32 = GetModuleHandleA("GDI32");
      if(gdi32) {
        PD3DKMTSetProcessSchedulingPriorityClass fn =
          (PD3DKMTSetProcessSchedulingPriorityClass)GetProcAddress(gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
        if(fn) {
          status = fn(GetCurrentProcess(), D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME);
          if(FAILED(status)) {
            // BOOST_LOG(warning) << "Failed to set realtime GPU priority. Please run application as administrator for optimal performance.";
          }
        }
      }

      directx::dxgi::Device dxgi;
      status = self->device->QueryInterface(IID_IDXGIDevice, (void **)&dxgi);
      if(FAILED(status)) {
        // BOOST_LOG(warning) << "Failed to query DXGI interface from device [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      dxgi->SetGPUThreadPriority(7);
    }

    // Try to reduce latency
    {
      directx::dxgi::Device1 dxgi = {0};
      status = self->device->QueryInterface(IID_IDXGIDevice, (void **)&dxgi);
      if(FAILED(status)) {
        LOG_ERROR("Failed to query DXGI interface from device ");
        return -1;
      }

      status = dxgi->SetMaximumFrameLatency(1);
      if(FAILED(status)) {
        LOG_ERROR("Failed to set maximum frame latency");
      }
    }

    //FIXME: Duplicate output on RX580 in combination with DOOM (2016) --> BSOD
    //TODO: Use IDXGIOutput5 for improved performance
    {
      directx::dxgi::Output1 output1 = {0};
      status = self->output->QueryInterface(IID_IDXGIOutput1, (void **)&output1);
      if(FAILED(status)) {
        LOG_ERROR("Failed to query IDXGIOutput1 from the output");
        return -1;
      }

      // We try this twice, in case we still get an error on reinitialization
      for(int x = 0; x < 2; ++x) {
        status = output1->DuplicateOutput((IUnknown *)self->device, &self->dup.dup);
        if(SUCCEEDED(status)) {
          break;
        }
        std::this_thread::sleep_for(200ms);
      }

      if(FAILED(status)) {
        LOG_ERROR("DuplicateOutput Failed [0x");
        return -1;
      }
      output1->Release();
    }

    DXGI_OUTDUPL_DESC dup_desc;
    self->dup.dup->GetDesc(&dup_desc);
    self->format = dup_desc.ModeDesc.Format;
    // BOOST_LOG(debug) << "Source format ["sv << format_str[dup_desc.ModeDesc.Format] << ']';
    return 0;
  }

  const char *format_str[] = {
    "DXGI_FORMAT_UNKNOWN",
    "DXGI_FORMAT_R32G32B32A32_TYPELESS",
    "DXGI_FORMAT_R32G32B32A32_FLOAT",
    "DXGI_FORMAT_R32G32B32A32_UINT",
    "DXGI_FORMAT_R32G32B32A32_SINT",
    "DXGI_FORMAT_R32G32B32_TYPELESS",
    "DXGI_FORMAT_R32G32B32_FLOAT",
    "DXGI_FORMAT_R32G32B32_UINT",
    "DXGI_FORMAT_R32G32B32_SINT",
    "DXGI_FORMAT_R16G16B16A16_TYPELESS",
    "DXGI_FORMAT_R16G16B16A16_FLOAT",
    "DXGI_FORMAT_R16G16B16A16_UNORM",
    "DXGI_FORMAT_R16G16B16A16_UINT",
    "DXGI_FORMAT_R16G16B16A16_SNORM",
    "DXGI_FORMAT_R16G16B16A16_SINT",
    "DXGI_FORMAT_R32G32_TYPELESS",
    "DXGI_FORMAT_R32G32_FLOAT",
    "DXGI_FORMAT_R32G32_UINT",
    "DXGI_FORMAT_R32G32_SINT",
    "DXGI_FORMAT_R32G8X24_TYPELESS",
    "DXGI_FORMAT_D32_FLOAT_S8X24_UINT",
    "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS",
    "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT",
    "DXGI_FORMAT_R10G10B10A2_TYPELESS",
    "DXGI_FORMAT_R10G10B10A2_UNORM",
    "DXGI_FORMAT_R10G10B10A2_UINT",
    "DXGI_FORMAT_R11G11B10_FLOAT",
    "DXGI_FORMAT_R8G8B8A8_TYPELESS",
    "DXGI_FORMAT_R8G8B8A8_UNORM",
    "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",
    "DXGI_FORMAT_R8G8B8A8_UINT",
    "DXGI_FORMAT_R8G8B8A8_SNORM",
    "DXGI_FORMAT_R8G8B8A8_SINT",
    "DXGI_FORMAT_R16G16_TYPELESS",
    "DXGI_FORMAT_R16G16_FLOAT",
    "DXGI_FORMAT_R16G16_UNORM",
    "DXGI_FORMAT_R16G16_UINT",
    "DXGI_FORMAT_R16G16_SNORM",
    "DXGI_FORMAT_R16G16_SINT",
    "DXGI_FORMAT_R32_TYPELESS",
    "DXGI_FORMAT_D32_FLOAT",
    "DXGI_FORMAT_R32_FLOAT",
    "DXGI_FORMAT_R32_UINT",
    "DXGI_FORMAT_R32_SINT",
    "DXGI_FORMAT_R24G8_TYPELESS",
    "DXGI_FORMAT_D24_UNORM_S8_UINT",
    "DXGI_FORMAT_R24_UNORM_X8_TYPELESS",
    "DXGI_FORMAT_X24_TYPELESS_G8_UINT",
    "DXGI_FORMAT_R8G8_TYPELESS",
    "DXGI_FORMAT_R8G8_UNORM",
    "DXGI_FORMAT_R8G8_UINT",
    "DXGI_FORMAT_R8G8_SNORM",
    "DXGI_FORMAT_R8G8_SINT",
    "DXGI_FORMAT_R16_TYPELESS",
    "DXGI_FORMAT_R16_FLOAT",
    "DXGI_FORMAT_D16_UNORM",
    "DXGI_FORMAT_R16_UNORM",
    "DXGI_FORMAT_R16_UINT",
    "DXGI_FORMAT_R16_SNORM",
    "DXGI_FORMAT_R16_SINT",
    "DXGI_FORMAT_R8_TYPELESS",
    "DXGI_FORMAT_R8_UNORM",
    "DXGI_FORMAT_R8_UINT",
    "DXGI_FORMAT_R8_SNORM",
    "DXGI_FORMAT_R8_SINT",
    "DXGI_FORMAT_A8_UNORM",
    "DXGI_FORMAT_R1_UNORM",
    "DXGI_FORMAT_R9G9B9E5_SHAREDEXP",
    "DXGI_FORMAT_R8G8_B8G8_UNORM",
    "DXGI_FORMAT_G8R8_G8B8_UNORM",
    "DXGI_FORMAT_BC1_TYPELESS",
    "DXGI_FORMAT_BC1_UNORM",
    "DXGI_FORMAT_BC1_UNORM_SRGB",
    "DXGI_FORMAT_BC2_TYPELESS",
    "DXGI_FORMAT_BC2_UNORM",
    "DXGI_FORMAT_BC2_UNORM_SRGB",
    "DXGI_FORMAT_BC3_TYPELESS",
    "DXGI_FORMAT_BC3_UNORM",
    "DXGI_FORMAT_BC3_UNORM_SRGB",
    "DXGI_FORMAT_BC4_TYPELESS",
    "DXGI_FORMAT_BC4_UNORM",
    "DXGI_FORMAT_BC4_SNORM",
    "DXGI_FORMAT_BC5_TYPELESS",
    "DXGI_FORMAT_BC5_UNORM",
    "DXGI_FORMAT_BC5_SNORM",
    "DXGI_FORMAT_B5G6R5_UNORM",
    "DXGI_FORMAT_B5G5R5A1_UNORM",
    "DXGI_FORMAT_B8G8R8A8_UNORM",
    "DXGI_FORMAT_B8G8R8X8_UNORM",
    "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM",
    "DXGI_FORMAT_B8G8R8A8_TYPELESS",
    "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB",
    "DXGI_FORMAT_B8G8R8X8_TYPELESS",
    "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB",
    "DXGI_FORMAT_BC6H_TYPELESS",
    "DXGI_FORMAT_BC6H_UF16",
    "DXGI_FORMAT_BC6H_SF16",
    "DXGI_FORMAT_BC7_TYPELESS",
    "DXGI_FORMAT_BC7_UNORM",
    "DXGI_FORMAT_BC7_UNORM_SRGB",
    "DXGI_FORMAT_AYUV",
    "DXGI_FORMAT_Y410",
    "DXGI_FORMAT_Y416",
    "DXGI_FORMAT_NV12",
    "DXGI_FORMAT_P010",
    "DXGI_FORMAT_P016",
    "DXGI_FORMAT_420_OPAQUE",
    "DXGI_FORMAT_YUY2",
    "DXGI_FORMAT_Y210",
    "DXGI_FORMAT_Y216",
    "DXGI_FORMAT_NV11",
    "DXGI_FORMAT_AI44",
    "DXGI_FORMAT_IA44",
    "DXGI_FORMAT_P8",
    "DXGI_FORMAT_A8P8",
    "DXGI_FORMAT_B4G4R4A4_UNORM",

    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

    "DXGI_FORMAT_P208",
    "DXGI_FORMAT_V208",
    "DXGI_FORMAT_V408"
  };
  void 
  cursor_set_pos(GpuCursor* self,
                 LONG rel_x, 
                 LONG rel_y, 
                 bool visible) 
  {
      self->cursor_view.TopLeftX = rel_x;
      self->cursor_view.TopLeftY = rel_y;

      self->visible = visible;
  }

  void 
  cursor_set_texture(GpuCursor* self,
                     LONG width, 
                     LONG height, 
                     directx::d3d11::Texture2D texture) 
  {
      self->cursor_view.Width  = width;
      self->cursor_view.Height = height;

      self->texture = texture;
  }

  GpuCursorClass*     
  gpu_cursor_class_init()
  {
    static bool init = FALSE;
    static GpuCursorClass klass = {0};
    if (init)
      return &klass;
    
    klass.set_pos = cursor_set_pos;
    klass.set_texture = cursor_set_texture;
    return &klass;
  }

    display::HLSL*
    init_hlsl() 
    {
      // BOOST_LOG(info) << "Compiling shaders..."sv;

      static bool initialize = false;
      display::HLSL* hlsl = (display::HLSL*)malloc(sizeof(display::HLSL));
      memset(hlsl,0,sizeof(display::HLSL));

      if (initialize)
        return hlsl;
      

      hlsl->scene_vs_hlsl = helper::compile_vertex_shader(SUNSHINE_SHADERS_DIR "/SceneVS.hlsl");
      if(!hlsl->scene_vs_hlsl) {
        return NULL;
      }

      hlsl->convert_Y_ps_hlsl = helper::compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ConvertYPS.hlsl");
      if(!hlsl->convert_Y_ps_hlsl) {
        return NULL;
      }

      hlsl->convert_UV_ps_hlsl = helper::compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ConvertUVPS.hlsl");
      if(!hlsl->convert_UV_ps_hlsl) {
        return NULL;
      }

      hlsl->convert_UV_vs_hlsl = helper::compile_vertex_shader(SUNSHINE_SHADERS_DIR "/ConvertUVVS.hlsl");
      if(!hlsl->convert_UV_vs_hlsl) {
        return NULL;
      }

      hlsl->scene_ps_hlsl = helper::compile_pixel_shader(SUNSHINE_SHADERS_DIR "/ScenePS.hlsl");
      if(!hlsl->scene_ps_hlsl) {
        return NULL;
      }
      // BOOST_LOG(info) << "Compiled shaders"sv;
      return hlsl;
    }    
} // namespace platf::dxgi

