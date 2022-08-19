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


#include <screencoder_session.h>
extern "C" {
#include <go_adapter.h>
}


namespace appsink
{

    void
    appsink_handle(sink::GenericSink* sink, 
                   util::Buffer* buf)
    {
        appsink::AppSink* app = (appsink::AppSink*)sink;
        QUEUE_ARRAY_CLASS->push(app->out,buf);
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
        static AppSink sink;
        RETURN_ONCE((sink::GenericSink*)&sink);
        
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






extern "C" {
#include <go_adapter.h>
}

#define DEFAULT_BITRATE 1000

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
    BUFFER_UNREF(buffer);
}

int go_shared_bitrate = DEFAULT_BITRATE;
void GoSetBitrate(int bitrate) { go_shared_bitrate = bitrate; }
int  GoGetBitrate()            { return go_shared_bitrate; }

static bool 
select_monitor (char* name)
{
    return TRUE;
}

void
InitScreencoder()
{
    session::start_session(GoGetBitrate,select_monitor,APP_SINK);
}