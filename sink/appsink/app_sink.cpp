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
#include <screencoder_adaptive.h>


#include <thread>
using namespace std::literals;


namespace appsink
{

    static void
    appsink_handle(sink::GenericSink* sink, 
                   util::Buffer* buf)
    {
        appsink::AppSink* app = (appsink::AppSink*)sink;
        QUEUE_ARRAY_CLASS->push(app->out,buf,true);
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

    util::QueueArray*
    get_input_event(sink::GenericSink* gen)
    {
        return ((AppSink*)gen)->sink_event_in;
    }

    util::QueueArray*
    get_output_event(sink::GenericSink* gen)
    {
        return ((AppSink*)gen)->sink_event_out;
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
        sink->base.get_input_eve = get_input_event;
        sink->base.get_output_eve = get_output_event;

        sink->sink_event_in = QUEUE_ARRAY_CLASS->init(INFINITE);;
        sink->sink_event_out = QUEUE_ARRAY_CLASS->init(INFINITE);
        sink->prev_pkt_sent = std::chrono::high_resolution_clock::now();
        sink->this_pkt_sent = std::chrono::high_resolution_clock::now();
        sink->out = QUEUE_ARRAY_CLASS->init(1);
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

    libav::Packet* pkt = (libav::Packet*)QUEUE_ARRAY_CLASS->pop(sink->out,(util::Buffer**)buf,size,true);
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

    BUFFER_MALLOC(evebuf,sizeof(adaptive::AdaptiveEvent),ptr);
    adaptive::AdaptiveEvent* eve = (adaptive::AdaptiveEvent*)ptr;
    eve->code = adaptive::AdaptiveEventCode::SINK_CYCLE_REPORT;
    eve->time_data = delta;
    QUEUE_ARRAY_CLASS->push(sink->sink_event_out,evebuf,true);

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

int
QueryEncoder(char* encoder_name)
{
    encoder::Encoder encoder;
    if (string_compare(encoder_name,"nvenc_hevc")) {
        encoder = NVENC("h265");
    } else if (string_compare(encoder_name,"nvenc_h264")) {
        encoder = NVENC("h264");
    } else if (string_compare(encoder_name,"amf_h264")) {
        encoder = AMD("h264");
    } else if (string_compare(encoder_name,"amf_h265")) {
        encoder = AMD("h265");
    } else {
        return 0;
    }

    return encoder.codec_config->capabilities[encoder::FrameFlags::PASSED];
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
    } else if (string_compare(encoder_name,"amf_h264")) {
        encoder = AMD("h264");
    } else if (string_compare(encoder_name,"amf_h265")) {
        encoder = AMD("h265");
    } else {
        LOG_ERROR("No available encoder");
    }
    
    if(!encoder.codec_config->capabilities[encoder::FrameFlags::PASSED]) {
        LOG_ERROR("NVENC encoder is not ready");
        return;
    }

    platf::Display* display = platf::get_display_by_name(&encoder,display_name);
    session::start_session(display,&encoder,
        (util::Broadcaster*)shutdown,
        (sink::GenericSink*)app_sink);
}
int   
StringLength(char* string)
{
    return strlen(string);
}