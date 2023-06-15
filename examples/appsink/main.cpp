/**
 * @file main.c
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-09-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */
extern "C" {
#include <go_adapter.h>
}

#include <thread>

void 
readloop(void* appsink)
{
    while (true)
    {
        int size, duration, res;
        void* data,*buf;
        res = GoHandleAVPacket(appsink,&data,&buf,&size,&duration);
        if (!res)
            return;
        
        GoUnrefAVPacket(appsink,buf);
    }
}

int main()
{
    void* queue_in = NewEvent();
    void* queue_out = NewEvent();
    void* appsink = AllocateAppSink();
    void* shutdown = NewEvent();
    std::thread handle {readloop,appsink};
    handle.detach();
    // StartScreencodeThread(appsink,shutdown,"nvenc_h264",QueryDisplay(0));
}