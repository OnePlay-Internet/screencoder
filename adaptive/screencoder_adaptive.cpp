/**
 * @file screencoder_adaptive.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-09-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_adaptive.h>
#include <screencoder_util.h>

#include <thread>
using namespace std::literals;

namespace adaptive 
{

    void
    push_record(AdaptiveContext* context, 
                Record* record)
    {
        for (int i = 1; i < 100; i++)
        {
            context->records[i] = context->records[i-1];
        }
        memcpy((pointer)&context->records[0],(pointer)record,sizeof(Record));
    }

    void
    median_10_record(AdaptiveContext* context, 
                     Record* record)
    {
        std::chrono::nanoseconds median_sink_cycle, median_capture_cycle;

        for (int i = 0; i < 10; i++)
        {
            median_sink_cycle += context->records[i].sink_cycle;
            median_capture_cycle += context->records[i].capture_cycle;
        }

        record->capture_cycle = median_capture_cycle / 10;
        record->sink_cycle = median_sink_cycle / 10;
        record->sink_queue_size = context->records[0].sink_queue_size;
        record->timestamp = std::chrono::high_resolution_clock::now();
    }

    void adaptiveThread(AdaptiveContext* context)
    {
        while (!IS_INVOKED(context->shutdown))
        {
            Record rec;
            bool has_sink,has_capture = false;

            std::this_thread::sleep_for(10ms);
            if (QUEUE_ARRAY_CLASS->peek(context->sink_event_in))
            {
                util::Buffer* buf = NULL;
                AdaptiveEvent* event = (AdaptiveEvent*) QUEUE_ARRAY_CLASS->pop(context->sink_event_in,&buf,NULL);

                switch (event->code)
                {
                case AdaptiveEventCode::SINK_LATENCY_REPORT:
                    rec.sink_cycle = event->time_data;
                    has_sink = true;
                    break;
                default:
                    LOG_ERROR("unknown event");
                    break;
                }



                BUFFER_UNREF(buf);
            }
            
            if (QUEUE_ARRAY_CLASS->peek(context->capture_event_in))
            {
                util::Buffer* buf = NULL;
                AdaptiveEvent* event = (AdaptiveEvent*) QUEUE_ARRAY_CLASS->pop(context->capture_event_in,&buf,NULL);

                switch (event->code)
                {
                case AdaptiveEventCode::CAPTURE_TIMEOUT:
                    /* code */
                    break;
                case AdaptiveEventCode::CAPTURE_CYCLE_REPORT :
                    rec.capture_cycle = event->time_data;
                    has_capture = true;
                    break;
                default:
                    LOG_ERROR("unknown event");
                    break;
                }


                BUFFER_UNREF(buf);
            }


            if (has_sink || has_capture)
            {
                rec.timestamp = std::chrono::high_resolution_clock::now();
                rec.sink_queue_size = QUEUE_ARRAY_CLASS->size(context->sink_queue);

                if(!has_sink) 
                    rec.sink_cycle = context->records[0].sink_cycle;
                if(!has_capture) 
                    rec.capture_cycle = context->records[0].capture_cycle;

                push_record(context,&rec);
            }
        }
    }

    void adaptiveThreadWatch(AdaptiveContext* context)
    {
        while (!IS_INVOKED(context->shutdown))
        {
            std::this_thread::sleep_for(3s);


            Record record;
            median_10_record(context,&record);

            
        }
    }
}


