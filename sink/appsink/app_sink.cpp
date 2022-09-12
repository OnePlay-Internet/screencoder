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

#include <encoder_datatype.h>


#include <thread>
using namespace std::literals;


namespace appsink
{

    static void
    appsink_handle(sink::GenericSink* sink, 
                   util::Buffer* buf)
    {
        appsink::AppSink* app = (appsink::AppSink*)sink;
        QUEUE_ARRAY_CLASS->push(app->out,buf);
    }

    static char*
    appsink_describe(sink::GenericSink* sink)
    {
        return "application sink";
    }

    static int
    appsink_start(sink::GenericSink* sink)
    {
        return 0;
    }

    static void
    appsink_stop(sink::GenericSink* sink)
    {
        AppSink* app = (AppSink*)sink;
        QUEUE_ARRAY_CLASS->stop(app->out);
        util::free_keyvalue_pairs(app->base.options);
        free((pointer)sink);
    }
    
    static void
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
        sink->base.stop      = appsink_stop;

        sink->out = QUEUE_ARRAY_CLASS->init();
        return (sink::GenericSink*)sink;
    }
    
} // namespace appsink






extern "C" {
#include <go_adapter.h>
}


int
GoHandleAVPacket(void* appsink_ptr,
                 void** data,
                 void** buf, 
                 int* size, 
                 int* duration)
{
    appsink::AppSink* sink = (appsink::AppSink*)appsink_ptr;

    if (!sink)
        return FALSE;
    
    QUEUE_ARRAY_CLASS->wait(sink->out);

    libav::Packet* pkt = (libav::Packet*)QUEUE_ARRAY_CLASS->pop(sink->out,(util::Buffer**)buf,size);
    int64 current = BUFFER_CLASS->created((util::Buffer*)*buf);

    *data = pkt->data;
    *size = pkt->size;

    static int64 prev = 0;
    *duration = current - prev;
    prev = BUFFER_CLASS->created((util::Buffer*)*buf);
    return TRUE;
}
    
void
GoUnrefAVPacket(void* appsink_ptr,
                void* buf)
{
    appsink::AppSink* sink = (appsink::AppSink*)appsink_ptr;

    sink->this_pkt_sent = std::chrono::high_resolution_clock::now(); 
    std::chrono::nanoseconds delta = sink->this_pkt_sent - sink->prev_pkt_sent;
    sink->prev_pkt_sent = sink->this_pkt_sent;

    

    util::Buffer* buffer = (util::Buffer*)buf;
    BUFFER_UNREF(buffer);
}


void*
AllocateAppSink() 
{
    return (void*)appsink::new_app_sink();
}

void
StopAppSink(void* sink) {
    sink::GenericSink* appsink = (sink::GenericSink*)sink;
    appsink->stop(appsink);
}


void*
NewEvent() 
{
    return (void*)NEW_EVENT;
}

void
RaiseEvent(void* event) 
{
    RAISE_EVENT((util::Broadcaster*)event);
}

char* 
QueryDisplay(int index)
{
    encoder::Encoder encoder = NVENC("h264");
    platf::Display** displays = display::get_all_display(&encoder);
    if (*(displays+index)) {
        return (*(displays+index))->name;
    } else {
        return "out of range";
    }
}



void
StartScreencodeThread(void* app_sink,
                void* shutdown,
                char* encoder_name,
                char* display_name)
{
    encoder::Encoder encoder;
    if (string_compare(encoder_name,"nvenc_hevc")) {
        encoder = NVENC("h265");
    } else if (string_compare(encoder_name,"nvenc_h264")) {
        encoder = NVENC("h264");
    }
    
    if(!encoder.codec_config->capabilities[encoder::FrameFlags::PASSED]) {
        LOG_ERROR("NVENC encoder is not ready");
        return;
    }

    platf::Display** displays = display::get_all_display(&encoder);

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
    return; 
start:
    session::start_session(display,&encoder,
        (util::Broadcaster*)shutdown,
        (sink::GenericSink*)app_sink);
}
int   
StringLength(char* string)
{
    return strlen(string);
}