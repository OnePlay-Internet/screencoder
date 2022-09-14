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


#include <gpu_hw_device.h>
#include <platform_common.h>
#include <d3d11_datatype.h>
#include <display_base.h>

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
        std::chrono::high_resolution_clock::time_point produced;
        DXGI_OUTDUPL_FRAME_INFO frame_info;
        pthread_mutex_t mutex;
        platf::Capture status;

        d3d11::Texture2D texture_temporary;
        gpu::ImageGpu img_permanent;
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

        HRESULT status = dup->dup->AcquireNextFrame(DEFAULT_TIMEOUT, &texture->frame_info, &texture->resource);
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
    seek_oldest_texture(Duplication* dup)
    {
        int value;
        TexturePool* pool = dup->pool;
        while(true)
        {
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::nanoseconds diff[TEXTURE_SIZE] = {0ns};
            for(int i = 0; i < TEXTURE_SIZE; i++) {
                if (pool->texture[i].status == platf::Capture::OK) {
                    diff[i] = now - pool->texture[i].produced;
                }
            }

            std::chrono::nanoseconds max = 0ns;
            for(int i = 0; i < TEXTURE_SIZE; i++) {
                if (diff[i] > max) {
                    max = diff[i];
                    value = i;
                    goto take;
                }
            }
            std::this_thread::sleep_for(1ms);
        }
        take:
        return value;
    }

    int 
    seek_latest_texture(Duplication* dup)
    {
        int value;
        TexturePool* pool = dup->pool;
        while(true)
        {
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::nanoseconds diff[TEXTURE_SIZE] = {0ns};
            for(int i = 0; i < TEXTURE_SIZE; i++) {
                if (pool->texture[i].status == platf::Capture::OK) {
                    diff[i] = now - pool->texture[i].produced;
                }
            }

            std::chrono::nanoseconds min = 0ns;
            for(int i = 0; i < TEXTURE_SIZE; i++) {
                if (diff[i] > min) {
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
    frame_produce_thread(Duplication* dup,
                         d3d11::DeviceContext ctx)
    {
        TexturePool* pool = dup->pool;
        std::chrono::high_resolution_clock::time_point prev = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point now  = std::chrono::high_resolution_clock::now();
        while(pool_check(pool)) {
            int count = seek_oldest_texture(dup);
            Texture* texture = &(pool->texture[count]);

            pthread_mutex_lock(&texture->mutex);
            texture->status = duplication_get_next_frame(dup,texture);
            if (texture->status != platf::Capture::OK)
                continue;
            
            texture->produced = std::chrono::high_resolution_clock::now();

            bool mouse_update_flag = texture->frame_info.LastMouseUpdateTime.QuadPart != 0 || 
                                     texture->frame_info.PointerShapeBufferSize > 0;

            bool frame_update_flag = texture->frame_info.AccumulatedFrames != 0 || 
                                     texture->frame_info.LastPresentTime.QuadPart != 0;

            bool update_flag       = mouse_update_flag || frame_update_flag;

            if(!update_flag) 
                texture->status = platf::Capture::TIMEOUT;

            if(frame_update_flag) {
                HRESULT result = texture->resource->QueryInterface(IID_ID3D11Texture2D, (void **)&texture->texture_temporary);
                if(FAILED(result)) {
                    LOG_ERROR("Couldn't copy dxgi resource to texture");
                    texture->status = platf::Capture::ERR;
                    continue;
                }
            }

            ctx->CopyResource(texture->img_permanent.texture, texture->texture_temporary);

            texture->status = duplication_release_frame(dup);
            if (texture->status != platf::Capture::OK)
                continue;

            pthread_mutex_unlock(&texture->mutex);

            {
                now  = std::chrono::high_resolution_clock::now();
                dup->cycle = now - prev;
                prev = now;
            }
        }
    }


    TexturePool*
    init_texture_pool(platf::Display* base)
    {
        TexturePool* pool = (TexturePool*) malloc (sizeof(TexturePool));
        memset(pool,0,sizeof(TexturePool));
        pool->overall_status = platf::Capture::OK;
        for (int i = 0; i < TEXTURE_SIZE; i++) {
            pool->texture[i].status = platf::Capture::NOT_READY;
            pool->texture[i].mutex = PTHREAD_MUTEX_INITIALIZER;
            base->klass->dummy_img(base,(platf::Image*)&pool->texture[i].img_permanent);
        }
        return pool;
    }


    Duplication*
    duplication_init(platf::Display* base,
                     dxgi::Output output, 
                     d3d11::Device device,
                     d3d11::DeviceContext device_ctx,
                     DXGI_FORMAT* format)
    {
        Duplication* dup = (Duplication*) malloc (sizeof(Duplication));
        memset(dup,0,sizeof(Duplication));
        dup->use_dwmflush = SCREENCODER_CONSTANT->dwmflush;
        dup->device_ctx = device_ctx;

        //FIXME: Duplicate output on RX580 in combination with DOOM (2016) --> BSOD
        //TODO: Use IDXGIOutput5 for improved performance
        // Enable DwmFlush() only if the current refresh rate can match the client framerate.
        {
            HRESULT status;
            dxgi::Output1 output1 = {0};
            status = output->QueryInterface(IID_IDXGIOutput1, (void **)&output1);
            if(FAILED(status)) {
                LOG_ERROR("Failed to query IDXGIOutput1 from the output");
                return NULL;
            }

            status = output1->DuplicateOutput((IUnknown*)device, &dup->dup);
            if(FAILED(status)) {
                LOG_ERROR("DuplicateOutput Failed");
                return NULL;
            }
            output1->Release();
        }

        DXGI_OUTDUPL_DESC dup_desc;
        dup->dup->GetDesc(&dup_desc);
        *format = dup_desc.ModeDesc.Format;

        dup->pool = init_texture_pool(base);
        std::thread thread {frame_produce_thread,dup,device_ctx};
        thread.detach();
        return dup;
    }


    void
    duplication_finalize(Duplication* dup) {
        dup->pool->overall_status = platf::Capture::STOPED;
        duplication_release_frame(dup);
        if (dup->dup) { dup->dup->Release(); }
        for (int i = 0; i < TEXTURE_SIZE; i++) {

        }

    }

    platf::Capture
    duplication_accquire_frame_from_pool (Duplication* dup,
                                          platf::Image*img)
    {
        gpu::ImageGpu* output = (gpu::ImageGpu*)img;

        int pos = seek_latest_texture(dup);
        if (dup->pool->overall_status != platf::Capture::OK)
            return dup->pool->overall_status;
        
        Texture* text = &dup->pool->texture[pos];
        pthread_mutex_lock(&text->mutex);
        dup->device_ctx->CopyResource(text->img_permanent.texture, 
                                      text->texture_temporary);
        pthread_mutex_unlock(&text->mutex);
        return platf::Capture::OK;
    }

    HRESULT
    duplication_get_availabe_frame_shape (Duplication* dup,
                                         uint8** pointer_shape_buffer,
                                         int* pointer_shape_buffer_size,
                                         DXGI_OUTDUPL_POINTER_SHAPE_INFO *pointer_shape_info)
    { 
        int pos = seek_latest_texture(dup);
        DXGI_OUTDUPL_FRAME_INFO* info = &dup->pool->texture[pos].frame_info;

        pthread_mutex_lock(&dup->pool->texture[pos].mutex);
        uint8* shape_pointer = (uint8*)malloc(info->PointerShapeBufferSize);
        *pointer_shape_buffer = shape_pointer;
        *pointer_shape_buffer_size = info->PointerShapeBufferSize;

        UINT dummy;
        HRESULT res = dup->dup->GetFramePointerShape(info->PointerShapeBufferSize, shape_pointer, &dummy, pointer_shape_info);
        pthread_mutex_lock(&dup->pool->texture[pos].mutex);
        return res;
    }

    DuplicationClass*
    duplication_class_init()
    {
        static DuplicationClass klass = {0};
        RETURN_PTR_ONCE(klass);

        klass.init           =   duplication_init;
        klass.next_frame     =   duplication_accquire_frame_from_pool;
        klass.get_cursor_buf =   duplication_get_availabe_frame_shape;
        klass.finalize       =   duplication_finalize;
        return &klass;
    }
  
} // namespace duplication