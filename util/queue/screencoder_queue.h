/**
 * @file queue.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#ifndef __VIDEO_CAPTURER_H__
#define __VIDEO_CAPTURER_H__


#include <screencoder_object.h>

#define QUEUE_ARRAY_CLASS       util::queue_array_class_init()


namespace util
{
    typedef struct _QueueArray QueueArray;

    typedef struct _QueueArrayClass{
        bool (*push) (QueueArray* queue, util::Buffer* data, bool record);

        void (*wait) (QueueArray* queue);

        bool (*peek) (QueueArray* queue);

        pointer       (*pop) (QueueArray* queue, 
                              util::Buffer** buf,
                              int* size,
                              bool record);
        QueueArray* (*init) (int max_size);

        uint64 (*size) (QueueArray* queue);

        void (*stop) (QueueArray* queue);
    } QueueArrayClass;
    
    QueueArrayClass*        queue_array_class_init      ();
} // namespace queue



#endif