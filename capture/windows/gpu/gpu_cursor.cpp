/**
 * @file gpu_cursor.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <gpu_cursor.h>
#include <screencoder_util.h>



namespace gpu
{

  void 
  cursor_set_pos(platf::Cursor* cur,                 
                 LONG rel_x, 
                 LONG rel_y) 
  {
      GpuCursor* self = (GpuCursor*)cur;
      self->cursor_view.TopLeftX = rel_x;
      self->cursor_view.TopLeftY = rel_y;
  }

  platf::CursorClass*     
  gpu_cursor_class_init()
  {
    static platf::CursorClass klass = {0};
    RETURN_PTR_ONCE(klass);
    
    klass.set_pos = cursor_set_pos;
    return &klass;
  }


    
} // namespace gpu
