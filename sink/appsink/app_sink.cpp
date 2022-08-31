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
#include <encoder_d3d11_device.h>
#include <display_base.h>

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
        AppSink* sink = (AppSink*)malloc(sizeof(AppSink));
        
        sink->base.name = "appsink";
        sink->base.options = util::new_keyvalue_pairs(1);
        sink->base.handle    = appsink_handle;
        sink->base.preset    = appsink_preset;
        sink->base.describe  = appsink_describe;
        sink->base.start     = appsink_start;

        sink->out = QUEUE_ARRAY_CLASS->init();
        return (sink::GenericSink*)&sink;
    }
    
} // namespace appsink






extern "C" {
#include <go_adapter.h>
}

#define DEFAULT_BITRATE 1000

int
GoHandleAVPacket(void* appsink_ptr,
                 void** data,
                 void** buf, 
                 int* size, 
                 int* duration)
{
    appsink::AppSink* sink = (appsink::AppSink*)appsink_ptr;

    static int64 prev = 0;
    if(QUEUE_ARRAY_CLASS->peek(sink->out)) {
        libav::Packet* pkt = (libav::Packet*)QUEUE_ARRAY_CLASS->pop(sink->out,(util::Buffer**)buf,size);
        int64 current = BUFFER_CLASS->created((util::Buffer*)*buf);

        *data = pkt->data;
        *size = pkt->size;
        *duration = current - prev;
        prev = BUFFER_CLASS->created((util::Buffer*)*buf);
        return 0;
    } else {
        return -1;
    }
}
    
void
GoUnrefAVPacket(void* buf){
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

void*
InitScreencoder(void* app_sink,
                void* shutdown,
                char* encoder_name,
                char* display_name,
                int bitrate)
{
    encoder::Encoder* encoder = NULL;
    if (string_compare(encoder_name,"nvenc_hevc")) {
        encoder = NVENC("h265");
    } else if (string_compare(encoder_name,"nvenc_h264")) {
        encoder = NVENC("h264");
    }
    
    if(!encoder) {
        LOG_ERROR("NVENC encoder is not ready");
        return NULL;
    }

    platf::Display** displays = display::get_all_display(encoder);

    int i =0;
    platf::Display* display;
    while (*(displays+i)) {
        if (string_compare((*(displays+i))->name,display_name)) {
            display = *(displays+i);
            goto start;
        }
        i++;
    }
    LOG_INFO("no match display");
    return NULL; 
start:
    session::start_session(display,encoder,
        (util::Broadcaster*)shutdown,
        (sink::GenericSink*)app_sink);
}