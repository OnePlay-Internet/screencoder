/**
 * @file display_ram.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __DISPLAY_RAM_H__
#define __DISPLAY_RAM_H__

#include <cpu_cursor.h>
#include <display_base.h>

namespace cpu
{    
    typedef struct _DisplayRam{
      display::DisplayBase base;
      Cursor cursor;

      D3D11_MAPPED_SUBRESOURCE img_info;
      d3d11::Texture2D texture;
    }DisplayRam;
    
} // namespace cpu



#endif