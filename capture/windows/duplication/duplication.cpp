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



namespace duplication
{
    platf::Capture    duplication_release_frame   (Duplication* dup) ;

    platf::Capture 
    duplication_get_next_frame(Duplication* dup,
                              std::chrono::milliseconds timeout, 
                              dxgi::Resource *res_p) 
    {
        platf::Capture return_status = duplication_release_frame(dup);
        if(return_status != platf::Capture::OK) 
            return return_status;
        
        if(dup->use_dwmflush) 
            DwmFlush();

        HRESULT status = dup->dup->AcquireNextFrame(timeout.count(), &dup->frame_info, res_p);
        if(status == S_OK) {
            dup->has_frame = true;
            return_status = platf::Capture::OK;
        } else if (status == DXGI_ERROR_WAIT_TIMEOUT){
            return_status = platf::Capture::TIMEOUT;
        } else if (status == WAIT_ABANDONED ||
                   status == DXGI_ERROR_ACCESS_LOST ||
                   status == DXGI_ERROR_ACCESS_DENIED) {
            dup->has_frame = false;
            return platf::Capture::REINIT;
        } else {
            return_status = platf::Capture::ERR;
        }
        return return_status;
    }


    platf::Capture 
    duplication_release_frame(Duplication* dup) 
    {
        if(!dup->has_frame) 
            return platf::Capture::OK;
        

        platf::Capture return_status;
        auto status = dup->dup->ReleaseFrame();
        if(status == S_OK) {
            dup->has_frame = false;
            return_status = platf::Capture::OK;
        } else if (status == DXGI_ERROR_WAIT_TIMEOUT){
            return_status = platf::Capture::TIMEOUT;
        } else if (status == WAIT_ABANDONED ||
                   status == DXGI_ERROR_ACCESS_LOST ||
                   status == DXGI_ERROR_ACCESS_DENIED) {
            dup->has_frame = false;
            return platf::Capture::REINIT;
        } else {
            LOG_ERROR("Couldn't release frame");
            return_status = platf::Capture::ERR;
        }
        return return_status;
    }

    void
    duplication_finalize(Duplication* dup) {
      duplication_release_frame(dup);
      if (dup->dup) { dup->dup->Release(); }
      
    }


    DuplicationClass*
    duplication_class_init()
    {
        static DuplicationClass klass = {0};
        RETURN_PTR_ONCE(klass);

        klass.next_frame    =   duplication_get_next_frame;
        klass.finalize      =   duplication_finalize;
        return &klass;
    }
  
} // namespace duplication