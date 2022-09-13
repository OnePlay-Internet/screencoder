/**
 * @file duplication.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_DUPLICATION_H__
#define __SUNSHINE_DUPLICATION_H__
#include <platform_common.h>
#include <duplication.h>
#include <d3d11_datatype.h>


#define DUPLICATION_CLASS           duplication::duplication_class_init()
    
namespace duplication
{
    typedef struct _Duplication {
        dxgi::OutputDuplication dup;
        bool has_frame;
        bool use_dwmflush;
        DXGI_OUTDUPL_FRAME_INFO frame_info;
    }Duplication;

    typedef struct _DuplicationClass {
        platf::Capture(*next_frame)     (Duplication* dup,
                                         std::chrono::milliseconds timeout, 
                                         dxgi::Resource* res_p);

        void          (*finalize)       (Duplication* dup);
    }DuplicationClass;


    DuplicationClass* duplication_class_init();
} // namespace duplication
 
#endif