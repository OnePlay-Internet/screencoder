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

int GoHandleAVPacket(void* appsink_ptr,
                     void** data,
                     void** buf, 
                     int* size, 
                     int* duration);

void GoUnrefAVPacket(void* appsink,
                     void* buf);

void StartScreencodeThread(void* app_sink,
                    void* shutdown,
                    char* encoder_name,
                    char* display_name);

char* QueryDisplay (int index);

void* AllocateAppSink();

void  StopAppSink     (void* data);

void* NewEvent();

void  RaiseEvent(void* event);


int   StringLength  (char* string);
#endif