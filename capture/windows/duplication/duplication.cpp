/**
 * @file duplication.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <duplication.h>
#include <screencoder_util.h>


#include <platform_common.h>
#include <d3d11_datatype.h>

#include <chrono>
#include <thread>
#include <screencoder_config.h>
using namespace std::literals;

#define TEXTURE_SIZE 10
#define DEFAULT_TIMEOUT ((std::chrono::microseconds)100ms).count()

namespace duplication
{
    typedef struct _Texture{
        dxgi::Resource resource;
        d3d11::Texture2D texture;
        std::chrono::high_resolution_clock::time_point produced;
        DXGI_OUTDUPL_FRAME_INFO frame_info;
        pthread_mutex_t mutex;
        platf::Capture status;
    }Texture;

    struct _TexturePool {
        Texture texture[TEXTURE_SIZE];
        platf::Capture overall_status;
    };

    platf::Capture 
    duplication_get_next_frame(Duplication* dup,
                              Texture* texture) 
    {
        platf::Capture return_status;
        if(dup->use_dwmflush) 
            DwmFlush();

        pthread_mutex_lock(&texture->mutex);
        HRESULT status = dup->dup->AcquireNextFrame(DEFAULT_TIMEOUT, &texture->frame_info, &texture->resource);
        pthread_mutex_unlock(&texture->mutex);

        if(status == S_OK) {
            return_status = platf::Capture::OK;
        } else if (status == DXGI_ERROR_WAIT_TIMEOUT){
            return_status = platf::Capture::TIMEOUT;
        } else if (status == WAIT_ABANDONED ||
                   status == DXGI_ERROR_ACCESS_LOST ||
                   status == DXGI_ERROR_ACCESS_DENIED) {
            return platf::Capture::REINIT;
        } else {
            return_status = platf::Capture::ERR;
        }
        return return_status;
    }


    platf::Capture 
    duplication_release_frame(Duplication* dup) 
    {
        platf::Capture return_status;
        auto status = dup->dup->ReleaseFrame();
        if(status == S_OK) {
            return_status = platf::Capture::OK;
        } else if (status == DXGI_ERROR_WAIT_TIMEOUT){
            return_status = platf::Capture::TIMEOUT;
        } else if (status == WAIT_ABANDONED ||
                   status == DXGI_ERROR_ACCESS_LOST ||
                   status == DXGI_ERROR_ACCESS_DENIED) {
            return platf::Capture::REINIT;
        } else {
            LOG_ERROR("Couldn't release frame");
            return_status = platf::Capture::ERR;
        }
        return return_status;
    }



    int 
    seek_available_texture(Duplication* dup)
    {
        int value;
        TexturePool* pool = dup->pool;
        while(true)
        {
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::nanoseconds diff[TEXTURE_SIZE] = {0ns};
            for(int i = 0; i < TEXTURE_SIZE; i++)
            {
                if (pool->texture[i].status == platf::Capture::OK) {
                    diff[i] = now - pool->texture[i].produced;
                }
            }

            std::chrono::nanoseconds min = 0ns;
            for(int i = 0; i < TEXTURE_SIZE; i++)
            {
                if (diff[i] > min)
                {
                    min = diff[i];
                    value = i;
                    goto take;
                }
            }
            std::this_thread::sleep_for(1ms);
        }
        take:
        return value;
    }


    bool
    pool_check(TexturePool* pool)
    {
        if (pool->overall_status == platf::Capture::STOPED)
            return false;

        for (int i = 0; i < TEXTURE_SIZE; i++)
        {
            platf::Capture status = pool->texture[i].status;
            if (status == platf::Capture::REINIT ||
                status == platf::Capture::ERR) {
                pool->overall_status = status;
                return false;
            }
        }
        return true;
    }

    void
    frame_produce_thread(Duplication* dup)
    {
        int count = 0;
        TexturePool* pool = dup->pool;
        while(pool_check(pool))
        {
            if (!dup->ready) {
                std::this_thread::sleep_for(50ms);
                continue;
            }


            Texture* texture = &pool->texture[count];
            texture->status = duplication_get_next_frame(dup,texture);
            if (texture->status != platf::Capture::OK)
                continue;
            
            texture->produced = std::chrono::high_resolution_clock::now();

            bool mouse_update_flag = texture->frame_info.LastMouseUpdateTime.QuadPart != 0 || texture->frame_info.PointerShapeBufferSize > 0;
            bool frame_update_flag = texture->frame_info.AccumulatedFrames != 0 || texture->frame_info.LastPresentTime.QuadPart != 0;
            bool update_flag       = mouse_update_flag || frame_update_flag;

            if(!update_flag) 
                texture->status = platf::Capture::TIMEOUT;

            if(frame_update_flag) {
                HRESULT result = texture->resource->QueryInterface(IID_ID3D11Texture2D, (void **)&texture->texture);
                if(FAILED(result)) {
                    LOG_ERROR("Couldn't copy dxgi resource to texture");
                    texture->status = platf::Capture::ERR;
                }
            }

            texture->status = duplication_release_frame(dup);
            if (texture->status != platf::Capture::OK)
                continue;

            count = (count == (TEXTURE_SIZE - 1)) ? 0 : count + 1;
        }
    }


    TexturePool*
    init_texture_pool()
    {
        TexturePool* pool = (TexturePool*) malloc (sizeof(TexturePool));
        memset(pool,0,sizeof(TexturePool));
        pool->overall_status = platf::Capture::OK;
        for (int i = 0; i < TEXTURE_SIZE; i++) {
            pool->texture[i].status = platf::Capture::NOT_READY;
            pool->texture[i].mutex = PTHREAD_MUTEX_INITIALIZER;
        }
    }


    Duplication*
    duplication_init()
    {
        Duplication* dup = (Duplication*) malloc (sizeof(Duplication));
        memset(dup,0,sizeof(Duplication));
        dup->use_dwmflush = SCREENCODER_CONSTANT->dwmflush;
        dup->pool = init_texture_pool();
        std::thread thread {frame_produce_thread,dup};
        thread.detach();
        return dup;
    }


    void
    duplication_finalize(Duplication* dup) {
        dup->pool->overall_status = platf::Capture::STOPED;
        duplication_release_frame(dup);
        if (dup->dup) { dup->dup->Release(); }
        for (int i = 0; i < TEXTURE_SIZE; i++) {
            if ( dup->pool->texture[i].texture ) 
                dup->pool->texture[i].texture->Release();
        }

    }

    platf::Capture
    duplication_accquire_frame_from_pool (Duplication* dup,
                                          d3d11::Texture2D* texture,
                                          pthread_mutex_t** mutex)
    {
        int pos = seek_available_texture(dup);
        if (dup->pool->overall_status != platf::Capture::OK)
            return dup->pool->overall_status;
        
        pthread_mutex_lock(&dup->pool->texture[pos].mutex);
        texture = &dup->pool->texture[pos].texture;
        *mutex = &dup->pool->texture[pos].mutex;
    }

    HRESULT
    duplication_get_availabe_frame_shape (Duplication* dup,
                                         uint8** pointer_shape_buffer,
                                         int* pointer_shape_buffer_size,
                                         DXGI_OUTDUPL_POINTER_SHAPE_INFO *pointer_shape_info)
    { 
        int pos = seek_available_texture(dup);
        DXGI_OUTDUPL_FRAME_INFO* info = &dup->pool->texture[pos].frame_info;

        uint8* shape_pointer = (uint8*)malloc(info->PointerShapeBufferSize);
        *pointer_shape_buffer = shape_pointer;
        *pointer_shape_buffer_size = info->PointerShapeBufferSize;

        UINT dummy;
        return dup->dup->GetFramePointerShape(info->PointerShapeBufferSize, shape_pointer, &dummy, pointer_shape_info);
    }

    DuplicationClass*
    duplication_class_init()
    {
        static DuplicationClass klass = {0};
        RETURN_PTR_ONCE(klass);

        klass.next_frame     =   duplication_accquire_frame_from_pool;
        klass.get_cursor_buf=   duplication_get_availabe_frame_shape;
        klass.finalize       =   duplication_finalize;
        return &klass;
    }
  
} // namespace duplication