/**
 * @file app_sink.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-08-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <app_sink.h>

extern "C" {
#include <go_adapter.h>
}

#include <screencoder_session.h>


namespace appsink
{
    void
    free_av_packet(void* pkt)
    {
        av_free_packet((libav::Packet*)pkt);
    }

    void
    appsink_handle(sink::GenericSink* sink, 
                   libav::Packet* pkt)
    {
        appsink::AppSink* app = (appsink::AppSink*)sink;
        util::Buffer* buf = BUFFER_CLASS->init(pkt,sizeof(libav::Packet),free_av_packet);
        QUEUE_ARRAY_CLASS->push(app->out,buf);
        BUFFER_CLASS->unref(buf);
    }

    char*
    appsink_describe(sink::GenericSink* sink)
    {
        return "application sink";
    }

    int
    appsink_start(sink::GenericSink* sink)
    {
        return 0;
    }
    
    void
    appsink_preset(sink::GenericSink* sink,
                   encoder::EncodeContext* encoder)
    {
        return ;
    }

    sink::GenericSink*    
    new_app_sink()
    {
        static bool init = false;
        static AppSink sink;
        if(init)
            return (sink::GenericSink*)&sink;
        
        sink.base.name = "appsink";
        sink.base.options = util::new_keyvalue_pairs(1);

        sink.base.handle    = appsink_handle;
        sink.base.preset    = appsink_preset;
        sink.base.describe  = appsink_describe;
        sink.base.start     = appsink_start;

        sink.out = QUEUE_ARRAY_CLASS->init();
        return (sink::GenericSink*)&sink;
    }
    
} // namespace appsink

int
GoHandleAVPacket(void** data,
                 void** buf, 
                 int* size, 
                 int* duration)
{
    appsink::AppSink* sink = (appsink::AppSink*)APP_SINK;
    if(QUEUE_ARRAY_CLASS->peek(sink->out)) {
        libav::Packet* pkt = (libav::Packet*)QUEUE_ARRAY_CLASS->pop(sink->out,(util::Buffer**)buf,size);
        *data = pkt->data;
        *size = pkt->size;
        *duration = pkt->duration;
        return 0;
    } else {
        return -1;
    }
}
    
int 
GoDestroyAVPacket(void* buf){
    util::Buffer* buffer = (util::Buffer*)buf;
    BUFFER_CLASS->unref(buffer);
}


void*
InitScreencoder()
{
    session::Session sess = {0};
    session::init_session(&sess);
    session::start_session(&sess);
    return (pointer)&sess;
}