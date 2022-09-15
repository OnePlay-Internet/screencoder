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
    typedef struct _Record {
        std::chrono::nanoseconds sink_cycle;

        std::chrono::nanoseconds capture_cycle;

        std::chrono::high_resolution_clock::time_point timestamp;

        uint64 sink_queue_size;
    }Record;
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
        Record records[1000];
        int record_count;
        std::chrono::high_resolution_clock::time_point prev;



        /**
         * @brief 
         * RAW processed data
         */
        std::chrono::nanoseconds capture_delay;
    }AdaptiveContext;


    typedef enum _AdaptiveEventCode {
        EVENT_NONE = 0,

        UPDATE_CAPTURE_DELAY_INTERVAL = 2,
        DISABLE_CAPTURE_DELAY_INTERVAL = 4,

        SINK_CYCLE_REPORT = 8,
        CAPTURE_CYCLE_REPORT = 16,
        CAPTURE_TIMEOUT = 32,

        AVCODEC_BITRATE_CHANGE = 64,
        AVCODEC_FRAMERATE_CHANGE = 128,
    }AdaptiveEventCode;

    typedef struct _AdaptiveEvent {
        AdaptiveEventCode code;
        std::chrono::nanoseconds time_data;
        int num_data;
    }AdaptiveEvent;



    void newAdaptiveControl (util::QueueArray* shutdown,
                             util::QueueArray* sink_queue,
                             util::QueueArray* capture_event_in,
                             util::QueueArray* capture_event_out,
                             util::QueueArray* sink_event_in,
                             util::QueueArray* sink_event_out
                             );
}



#endif