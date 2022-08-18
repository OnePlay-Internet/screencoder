/**
 * @file go_adapter.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-08-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __GO_ADAPTER_H__
#define __GO_ADAPTER_H__

int GoHandleAVPacket(void** data,
                     void** buf, 
                     int* size, 
                     int* duration);

int GoDestroyAVPacket(void* buf);

void InitScreencoder();

void GoSetBitrate(int bitrate);


#endif