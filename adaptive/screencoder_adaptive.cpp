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


#define EVALUATION_INTERVAL 3s

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

    void 
    adaptiveThreadListen(AdaptiveContext* context)
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

    void 
    adaptiveThreadProcess(AdaptiveContext* context)
    {
        while (!IS_INVOKED(context->shutdown))
        {
            AdaptiveEventCode code = AdaptiveEventCode::EVENT_NONE;

            Record record;
            median_10_record(context,&record);
            std::chrono::nanoseconds diff = record.sink_cycle - record.capture_cycle;
            std::chrono::nanoseconds buffer_size_in_time = record.sink_queue_size * record.capture_cycle;

            if ( buffer_size_in_time > 100ms) {
                /**
                 * @brief 
                 * network condition is bad, 
                 * buffered queue already been overloaded
                 */

                /**
                 * @brief 
                 * ASSUME that sink_cycle and capture_cycle is constant
                 * must ensure all element in queue is poped out during the next interval
                 * find capture delay which
                 * SATISFY
                 * A> var new_diff = ( sink_cycle - ( native_capture_cycle + capture_delay ))
                 * B> evaluation_interval / new_diff = -sink_queue_size 
                 */
                std::chrono::nanoseconds new_diff = -( EVALUATION_INTERVAL / record.sink_queue_size); 
                context->capture_delay = ( record.sink_cycle - new_diff ) - record.capture_cycle; 
            } else if (diff > 10ms ) {
                /**
                 * @brief 
                 * network condition is bad, 
                 * buffered queue might be overloaded
                 */
                context->capture_delay = diff;
            } else if (diff < 0ms) {
                /**
                 * @brief 
                 * ignore since network condition is good
                 */
                if(context->capture_delay != 0ms)
                    code = AdaptiveEventCode::DISABLE_CAPTURE_DELAY_INTERVAL;
                
                context->capture_delay = 0ms;
            } else {
                /**
                 * @brief 
                 * network condition is ok,
                 * do nothing
                 */
            }

            if (code != AdaptiveEventCode::EVENT_NONE)
            {
                BUFFER_MALLOC(buf,sizeof(AdaptiveEvent),ptr);
                AdaptiveEvent* event = (AdaptiveEvent*)ptr;
                event->code = code;


                QUEUE_ARRAY_CLASS->push(context->capture_event_out,buf);
                BUFFER_UNREF(buf);
            }

            std::this_thread::sleep_for(EVALUATION_INTERVAL);
        }
    }

    void newAdaptiveControl (util::QueueArray* sink_queue,
                             util::QueueArray* capture_event_in,
                             util::QueueArray* capture_event_out,
                             util::QueueArray* sink_event_in,
                             util::QueueArray* sink_event_out)
    {
        AdaptiveContext context;

        std::thread listen { adaptiveThreadListen, &context };
        listen.detach();
        std::thread process { adaptiveThreadProcess, &context };
        process.detach();
    }
}


