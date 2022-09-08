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
#include <screencoder_util.h>

#include <cmath>
#include <codecvt>

#include <display_base.h>
#include <screencoder_config.h>

#include <platform_common.h>
#include <encoder_device.h>



#include <d3d11_datatype.h>
#include <windows_helper.h>
#include <thread>
#include <cstdlib>
#include <string.h>

using namespace std::literals;


namespace display{
    // display selection
    platf::Display**
    get_all_display(encoder::Encoder* encoder)
    {
        static platf::Display* displays[10] = {0};
        char** display_names  = platf::display_names(helper::map_dev_type(encoder->dev_type));

        int count = 0;
        static char blacklist[10][100] = {0};
        while (*(display_names+count)) {
            platf::Display* display = NULL;
            char* name = *(display_names+count);

            if(!strlen(name))
              break;

            int y = 0;
            while (displays[y]) { // filter displays that already exist
                if (string_compare(displays[y]->name,name))
                  goto next;
                y++;
            }

            for(int z = 0; z< 10; z++) { // filter display that already fail to create resources
                if (string_compare(blacklist[z],name))
                    goto next;
            }

            // get display by name if its name does not exist in display list
            display = platf::get_display_by_name(helper::map_dev_type(encoder->dev_type), name);
            if(!display) {
                LOG_ERROR("unable to create display");
                for(int z = 0; z< 10; z++) {
                    if (!strlen(blacklist[z])) {
                        memcpy(blacklist[z],name,strlen(name));
                        break;
                    }
                }
            } else {
                y=0;
                while(displays[y]) 
                    y++;
                
                displays[y] = display;
            }
        next:
            count++;
        }

        return displays;
    }


    typedef enum _D3DKMT_SCHEDULINGPRIORITYCLASS {
        D3DKMT_SCHEDULINGPRIORITYCLASS_IDLE,
        D3DKMT_SCHEDULINGPRIORITYCLASS_BELOW_NORMAL,
        D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL,
        D3DKMT_SCHEDULINGPRIORITYCLASS_ABOVE_NORMAL,
        D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH,
        D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME
    } D3DKMT_SCHEDULINGPRIORITYCLASS;

    typedef NTSTATUS WINAPI (*PD3DKMTSetProcessSchedulingPriorityClass)(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS);








    HDESK 
    syncThreadDesktop() 
    {
        auto hDesk = OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, FALSE, GENERIC_ALL);
        if(!hDesk) {
            auto err = GetLastError();
            LOG_ERROR("Failed to Open Input Desktop");
            return nullptr;
        }

        if(!SetThreadDesktop(hDesk)) {
            auto err = GetLastError();
            LOG_ERROR("Failed to sync desktop to thread");
        }

        CloseDesktop(hDesk);

        return hDesk;
    }


    int
    display_base_init(DisplayBase* self,
                      char* display_name) 
    {
        // Ensure we can duplicate the current display
        syncThreadDesktop();

        platf::Display* disp = (platf::Display*)self;

        // Get rectangle of full desktop for absolute mouse coordinates
        disp->env_width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        disp->env_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        HRESULT status;

        status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **)&self->factory);
        if(FAILED(status)) {
            LOG_ERROR("Failed to create DXGIFactory1");
            return -1;
        }

        dxgi::Adapter adapter_p;
        for(int x = 0; self->factory->EnumAdapters1(x, &adapter_p) != DXGI_ERROR_NOT_FOUND; ++x) {
            DXGI_ADAPTER_DESC1 adapter_desc;
            dxgi::Adapter adapter_temp = adapter_p;
            adapter_temp->GetDesc1(&adapter_desc);

            dxgi::Output output;
            for(int y = 0; adapter_temp->EnumOutputs(y, &output) != DXGI_ERROR_NOT_FOUND; ++y) {
                DXGI_OUTPUT_DESC desc;
                output->GetDesc(&desc);

                char str2[100] = {0};
                wcstombs(str2, desc.DeviceName, 100);
                if(!string_compare(str2,display_name)) {
                    continue;
                }

                if(desc.AttachedToDesktop) {
                    self->output = output;

                    platf::Display* disp = (platf::Display*)self;
                    disp->offset_x = desc.DesktopCoordinates.left;
                    disp->offset_y = desc.DesktopCoordinates.top;
                    disp->width    = desc.DesktopCoordinates.right -  disp->offset_x;
                    disp->height   = desc.DesktopCoordinates.bottom - disp->offset_y;

                    // left and bottom may be negative, yet absolute mouse coordinates start at 0x0
                    // Ensure offset starts at 0x0
                    disp->offset_x -= GetSystemMetrics(SM_XVIRTUALSCREEN);
                    disp->offset_y -= GetSystemMetrics(SM_YVIRTUALSCREEN);
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
            LOG_ERROR("Failed to query IDXGIAdapter interface");
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
            LOG_ERROR("Failed to create D3D11 device");
            return -1;
        }


        // Enable DwmFlush() only if the current refresh rate can match the client framerate.
        self->dup.use_dwmflush = SCREENCODER_CONSTANT->dwmflush;
        {
            DWM_TIMING_INFO timing_info;
            timing_info.cbSize = sizeof(timing_info);
            status = DwmGetCompositionTimingInfo(NULL, &timing_info);
            if(FAILED(status)) {
                LOG_ERROR("Failed to detect active refresh rate");
                return -1;
            } else {
                platf::Display* disp = (platf::Display*)self;
                disp->framerate = std::round(
                    (double)timing_info.rateRefresh.uiNumerator / 
                    (double)timing_info.rateRefresh.uiDenominator);
            }
        }

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
                    LOG_WARNING("Could not set privilege to increase GPU priority");
                }
            }

            CloseHandle(token);

            HMODULE gdi32 = GetModuleHandleA("GDI32");
            if(gdi32) {
                PD3DKMTSetProcessSchedulingPriorityClass fn = (PD3DKMTSetProcessSchedulingPriorityClass)GetProcAddress(gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
                if(fn) {
                    status = fn(GetCurrentProcess(), D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME);
                    if(FAILED(status)) {
                    LOG_WARNING("Failed to set realtime GPU priority. Please run application as administrator for optimal performance.");
                    }
                }
            }

            dxgi::Device dxgi;
            status = self->device->QueryInterface(IID_IDXGIDevice, (void **)&dxgi);
            if(FAILED(status)) {
                LOG_WARNING("Failed to query DXGI interface from device");
                return -1;
            }

            dxgi->SetGPUThreadPriority(7);
        }

        // Try to reduce latency
        {
            dxgi::Device1 dxgi = {0};
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
            dxgi::Output1 output1 = {0};
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
                LOG_ERROR("DuplicateOutput Failed");
                return -1;
            }
            output1->Release();
        }

        DXGI_OUTDUPL_DESC dup_desc;
        self->dup.dup->GetDesc(&dup_desc);
        self->format = dup_desc.ModeDesc.Format;
        return 0;
    }
} // namespace platf::dxgi

