#include <common.h>
#include <duplication.h>
#include <d3d11_datatype.h>
    
namespace duplication
{
    typedef struct _Duplication {
        directx::dxgi::OutputDuplication dup;
        bool has_frame;
        bool use_dwmflush;
    }Duplication;

    typedef struct _DuplicationClass {
        platf::Capture(*next_frame)     (Duplication* dup,
                                         DXGI_OUTDUPL_FRAME_INFO &frame_info, 
                                         std::chrono::milliseconds timeout, 
                                         directx::dxgi::Resource res_p);

        platf::Capture(*reset)          (Duplication* dup,
                                         directx::dxgi::OutputDuplication* dup_p);

        platf::Capture(*release_frame)  (Duplication* dup);

        void          (*finalize)       (Duplication* dup);
    }DuplicationClass;


    DuplicationClass* duplication_class_init();
} // namespace duplication
 