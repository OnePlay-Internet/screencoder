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
#ifndef __SCREENCODER_ADAPTIVE_H__
#define __SCREENCODER_ADAPTIVE_H__

#include <screencoder_util.h>

namespace adaptive 
{
    typedef struct _AdaptiveContext {
        /**
         * @brief 
         * INPUT
         */
        util::QueueArray* sink_event_in;

        /**
         * @brief 
         * INPUT
         */
        util::QueueArray* capture_event_in;

        /**
         * @brief 
         * OUTPUT
         */
        util::QueueArray* capture_event_out;

        /**
         * @brief 
         * OUTPUT
         */
        util::QueueArray* sink_event_out;

        /**
         * @brief 
         * don't touch this
         */
        util::QueueArray* sink_queue;
        util::Broadcaster* shutdown;

        /**
         * @brief 
         * store metric as record
         */
        Record records[100];



        /**
         * @brief 
         * RAW processed data
         */
        std::chrono::nanoseconds capture_delay;
    }AdaptiveContext;

    typedef struct _Record {
        std::chrono::nanoseconds sink_cycle;

        std::chrono::nanoseconds capture_cycle;

        std::chrono::high_resolution_clock::time_point timestamp;

        uint64 sink_queue_size;
    }Record;

    typedef enum _AdaptiveEventCode {
        EVENT_NONE,

        UPDATE_CAPTURE_DELAY_INTERVAL,
        DISABLE_CAPTURE_DELAY_INTERVAL,

        SINK_LATENCY_REPORT,
        CAPTURE_CYCLE_REPORT,
        CAPTURE_TIMEOUT,

        AVCODEC_BITRATE_CHANGE,
        AVCODEC_FRAMERATE_CHANGE,
    }AdaptiveEventCode;

    typedef struct _AdaptiveEvent {
        AdaptiveEventCode code;

        uint8 data_byte[100];
        int size;

        std::chrono::nanoseconds time_data;
        int num_data;
    }AdaptiveEvent;



    void newAdaptiveControl (util::QueueArray* sink_queue,
                             util::QueueArray* capture_event_in,
                             util::QueueArray* capture_event_out,
                             util::QueueArray* sink_event_in,
                             util::QueueArray* sink_event_out);
}



#endif