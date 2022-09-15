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


#define DISABLE_EVALUATION true
#define EVALUATION_INTERVAL 3s
#define BUFFER_LIMIT 3ms

namespace adaptive 
{

    void
    push_record(AdaptiveContext* context, 
                Record* record)
    {
        for (int i = 1; i < RECORD_SIZE; i++)
        {
            context->records[i] = context->records[i-1];
        }
        memcpy((pointer)&context->records[0],(pointer)record,sizeof(Record));
    }

    void
    median_10_record(AdaptiveContext* context, 
                     Record* record)
    {
        int median_sink_cycle, median_capture_cycle, total_time, diff;

        for (int i = 0; i < RECORD_SIZE; i++) {
            diff = (context->records[i].timestamp - context->prev).count(); //miliseconds
            if (diff < 0)
                continue;
            
            median_sink_cycle    += context->records[i].sink_cycle.count() * diff;
            median_capture_cycle += context->records[i].capture_cycle.count() * diff;

            total_time += diff;
        }

        record->capture_cycle = (median_capture_cycle / total_time) * 1ns;
        record->sink_cycle =    (median_sink_cycle / total_time) * 1ns;
        record->sink_queue_size = context->records[0].sink_queue_size;
    }

    void 
    adaptiveThreadListen(AdaptiveContext* context)
    {
        while (!IS_INVOKED(context->shutdown))
        {
            std::this_thread::sleep_for(10ms);
            Record rec; memset(&rec,0,sizeof(Record));
            bool has_sink,has_capture = false;

            std::this_thread::sleep_for(10ms);
            if (QUEUE_ARRAY_CLASS->peek(context->sink_event_in))
            {
                util::Buffer* buf = NULL;
                AdaptiveEvent* event = (AdaptiveEvent*) QUEUE_ARRAY_CLASS->pop(context->sink_event_in,&buf,NULL,true);

                switch (event->code)
                {
                case AdaptiveEventCode::SINK_CYCLE_REPORT:
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
                AdaptiveEvent* event = (AdaptiveEvent*) QUEUE_ARRAY_CLASS->pop(context->capture_event_in,&buf,NULL,true);

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

                rec.sink_queue_size = QUEUE_ARRAY_CLASS->size(context->sink_queue);
                push_record(context,&rec);
                context->record_count++;
            }
        }
    }

    void 
    adaptiveThreadProcess(AdaptiveContext* context)
    {
        while (!IS_INVOKED(context->shutdown))
        {
            if (context->record_count < RECORD_SIZE) {
               std::this_thread::sleep_for(EVALUATION_INTERVAL); 
               continue;
            }
            
            AdaptiveEventCode code = AdaptiveEventCode::EVENT_NONE;

            Record record;
            median_10_record(context,&record);

            if (DISABLE_EVALUATION) {
                char log[100] = {0};
                int64 cap_mili =      std::chrono::duration_cast<std::chrono::milliseconds>(record.capture_cycle).count() %1000 %1000;
                int64 cap_micro =     std::chrono::duration_cast<std::chrono::microseconds>(record.capture_cycle).count() % 1000;
                int64 sink_mili =     std::chrono::duration_cast<std::chrono::milliseconds>(record.sink_cycle).count() % 1000 % 1000;
                int64 sink_micro =    std::chrono::duration_cast<std::chrono::microseconds>(record.sink_cycle).count() % 1000;
                snprintf(log,100,"capture cycle %dms,%dus | sink sycle %dms,%dus | buffer in queue %d",cap_mili,cap_micro,sink_mili,sink_micro,record.sink_queue_size);
                LOG_INFO(log);
            }

            context->prev = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_for(EVALUATION_INTERVAL);
        }
    }

    void newAdaptiveControl (util::QueueArray* shutdown,
                             util::QueueArray* sink_queue,
                             util::QueueArray* capture_event_in,
                             util::QueueArray* capture_event_out,
                             util::QueueArray* sink_event_in,
                             util::QueueArray* sink_event_out)
    {
        AdaptiveContext context;
        memset(&context,0,sizeof(AdaptiveContext));
        context.capture_event_in = capture_event_out;
        context.capture_event_out = capture_event_in;
        context.sink_event_in = sink_event_out;
        context.sink_event_out = sink_event_in;
        context.record_count=0;
        context.sink_queue = sink_queue;
        context.shutdown = shutdown;

        std::thread listen { adaptiveThreadListen, &context };
        listen.detach();
        std::thread process { adaptiveThreadProcess, &context };
        process.detach();
        WAIT_EVENT(context.shutdown);
    }
}


