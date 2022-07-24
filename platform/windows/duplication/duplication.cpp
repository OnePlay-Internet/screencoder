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
#include <sunshine_util.h>


#include <platform_common.h>



namespace duplication
{
    platf::Capture    duplication_release_frame   (Duplication* dup) ;

    platf::Capture 
    duplication_get_next_frame(Duplication* dup,
                              DXGI_OUTDUPL_FRAME_INFO* frame_info, 
                              std::chrono::milliseconds timeout, 
                              dxgi::Resource *res_p) 
    {
      platf::Capture capture_status = duplication_release_frame(dup);
      if(capture_status != platf::Capture::ok) {
        return capture_status;
      }

      if(dup->use_dwmflush) {
        DwmFlush();
      }

      HRESULT status = dup->dup->AcquireNextFrame(timeout.count(), frame_info, res_p);

      switch(status) {
      case S_OK:
        dup->has_frame = true;
        return platf::Capture::ok;
      case DXGI_ERROR_WAIT_TIMEOUT:
        return platf::Capture::timeout;
      case WAIT_ABANDONED:
      case DXGI_ERROR_ACCESS_LOST:
      default:
        LOG_ERROR("Couldn't acquire next frame");
        return platf::Capture::error;
      }
    }


    platf::Capture 
    duplication_release_frame(Duplication* dup) 
    {
        if(!dup->has_frame) {
          return platf::Capture::ok;
        }

        auto status = dup->dup->ReleaseFrame();
        switch(status) {
        case S_OK:
          dup->has_frame = false;
          return platf::Capture::ok;
        case DXGI_ERROR_WAIT_TIMEOUT:
          return platf::Capture::timeout;
        case WAIT_ABANDONED:
        case DXGI_ERROR_ACCESS_LOST:
        default:
          LOG_ERROR("Couldn't release frame");
          return platf::Capture::error;
        }
    }

    void
    duplication_finalize(Duplication* dup) {
      duplication_release_frame(dup);
    }


    DuplicationClass*
    duplication_class_init()
    {
      static bool initialize = false;
      static DuplicationClass klass = {0};
      if (initialize)
        return &klass;
      

      klass.next_frame    =   duplication_get_next_frame;
      klass.finalize      =   duplication_finalize;
      initialize = true;
      return &klass;
    }
  
} // namespace duplication