/**
 * @file encoder_d3d11_device.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __ENCODER_D3D11_DEVICE_H__
#define __ENCODER_D3D11_DEVICE_H__
#include <encoder_datatype.h>
#include <avcodec_datatype.h>
#include <sunshine_log.h>
#include <sunshine_macro.h>
#include <common.h>

#ifdef _WIN32
extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}
#endif

#define NVENC encoder::make_d3d11_encoder()

encoder::Encoder* encoder::make_d3d11_encoder();

#endif