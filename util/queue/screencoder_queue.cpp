/**
 * @file queue.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_queue.h>
#include <screencoder_macro.h>
#define BASE_SIZE 1024


#include <thread>
#include <stdio.h>
#include <string.h>
using namespace std::literals;

namespace util {
    /**
     * @brief 
     * Circular Array
     */
    struct _QueueArray {
        /**
         * @brief 
         * 
         */
        BufferLL* first;

        uint64 size;
    };




    /**
     * @brief 
     * 
     * @param queue 
     * @param data 
     * @return true 
     * @return false 
     */
    bool            
    queue_array_push(QueueArray* queue, 
                     util::Buffer* obj)
    {
        BufferLL* last = (BufferLL*)malloc(sizeof(BufferLL));
        memset(last,0,sizeof(BufferLL));

        BUFFER_REF(obj,NULL);
        last->obj  = obj;
        last->next = NULL;

        if(!queue->first) {
            queue->first = last;
        } else {
            BufferLL* container = queue->first;
            while (container->next) { container = container->next; }
            container->next = last;
        }

        queue->size++;
        return true;
    }

    bool
    queue_array_peek(QueueArray* queue)
    {
        return queue->first ? true : false;
    }

    uint64
    queue_array_size(QueueArray* queue)
    {
        return queue->size;
    }

    void
    queue_array_wait(QueueArray* queue)
    {
        while(!queue->first) {
            std::this_thread::sleep_for(5ms); // decrease sleep interval cause cpu consumption ramp up
        }
    }


    pointer
    queue_array_pop(QueueArray* queue, 
                    util::Buffer** buf,
                    int* size)
    {
        if (!queue_array_peek(queue))
            return NULL;

        BufferLL* container = queue->first;
        Buffer *ret = container->obj;
        queue->first = container->next;
        free(container);

        *buf = ret;
        pointer data = BUFFER_REF(ret,size);
        BUFFER_UNREF(ret);
        queue->size--;
        return data;
    }


    QueueArray*     
    queue_array_init()
    {
        QueueArray* array = (QueueArray*)malloc(sizeof(QueueArray));
        array->first = NULL;
        array->size = 0;
        return array;
    }


    void            
    queue_array_finalize(QueueArray* queue)
    {
        while(queue_array_peek(queue))
        {
            util::Buffer* buf;
            queue_array_pop(queue,&buf,NULL);
            BUFFER_UNREF(buf);
        }

        free(queue);
    }

    /**
     * 
     * @brief 
     * 
     */
    QueueArrayClass*
    queue_array_class_init()
    {
        static QueueArrayClass klass = {0};
        RETURN_PTR_ONCE(klass);

        klass.init = queue_array_init;
        klass.size = queue_array_size;
        klass.wait = queue_array_wait;
        klass.peek = queue_array_peek;
        klass.pop  = queue_array_pop;
        klass.push = queue_array_push;
        klass.stop = queue_array_finalize;
        return &klass;
    }
}