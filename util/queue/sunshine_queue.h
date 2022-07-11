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


#include <sunshine_object.h>

#define QUEUE_ARRAY_CLASS       util::queue_array_class_init()

namespace util
{
    typedef struct _QueueArray QueueArray;

    typedef struct _QueueArrayClass{
        bool (*push) (QueueArray* queue, util::Object* data);

        bool (*peek) (QueueArray* queue);

        util::Object* (*pop) (QueueArray* queue);

        QueueArray* (*init) ();

        void (*stop) (QueueArray* queue);
    } QueueArrayClass;
    
    QueueArrayClass*        queue_array_class_init      ();
} // namespace queue



#endif