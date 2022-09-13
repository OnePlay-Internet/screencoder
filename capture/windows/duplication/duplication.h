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
#include <d3d11_datatype.h>

#include <pthread.h>

#define DUPLICATION_CLASS           duplication::duplication_class_init()
    
namespace duplication
{
    typedef struct _TexturePool TexturePool;

    typedef struct _Duplication {
        dxgi::OutputDuplication dup;
        bool use_dwmflush;
        bool ready;
        TexturePool* pool;
    }Duplication;

    typedef struct _DuplicationClass {
        platf::Capture(*next_frame)     (Duplication* dup,
                                         d3d11::Texture2D* texture,
                                         pthread_mutex_t** mutex);

        HRESULT       (*get_cursor_buf) (Duplication* dup,
                                         uint8** pointer_shape_buffer,
                                         int* pointer_shape_buffer_size,
                                         DXGI_OUTDUPL_POINTER_SHAPE_INFO *pointer_shape_info);

        void          (*finalize)       (Duplication* dup);

        Duplication*  (*init)           ();
    }DuplicationClass;


    DuplicationClass* duplication_class_init();
} // namespace duplication
 
#endif