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
                     d3d11::Texture2D texture) 
  {
      self->cursor_view.Width  = width;
      self->cursor_view.Height = height;
      self->texture = texture;
  }

  GpuCursorClass*     
  gpu_cursor_class_init()
  {
    static GpuCursorClass klass = {0};
    RETURN_PTR_ONCE(klass);
    
    klass.set_pos = cursor_set_pos;
    klass.set_texture = cursor_set_texture;
    return &klass;
  }


    
} // namespace gpu
