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


#include <pthread.h>

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

        pthread_mutex_t mutex;
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
                     util::Buffer* obj,
                     bool record)
    {
        BufferLL* last = (BufferLL*)malloc(sizeof(BufferLL));
        memset(last,0,sizeof(BufferLL));


        if (record)
        {
                
#ifndef MINGW
        util::object_class_init()->ref(obj,NULL,"\\screencoder_queue.cpp",__LINE__);
#else
        util::object_class_init()->ref(obj,NULL,"/screencoder_queue.cpp",__LINE__);
#endif
        } else {
            util::object_class_init()->ref(obj,NULL,"/util/",__LINE__);
        }

        // lock this
        pthread_mutex_lock(&queue->mutex);
        {
            last->obj  = obj;
            last->next = NULL;

            if(!queue->first) {
                queue->first = last;
            } else {
                BufferLL* container = queue->first;
                while (container->next) { container = container->next; }
                container->next = last;
            }
        }
        pthread_mutex_unlock(&queue->mutex);

        queue->size++;
        return true;
    }

    bool
    queue_array_peek(QueueArray* queue)
    {
        return queue->size > 0 ? true : false;
    }

    uint64
    queue_array_size(QueueArray* queue)
    {
        return queue->size;
    }

    void
    queue_array_wait(QueueArray* queue)
    {
        while(!queue->size) {
            std::this_thread::sleep_for(1ms); // decrease sleep interval cause cpu consumption ramp up
        }
    }


    pointer
    queue_array_pop(QueueArray* queue, 
                    util::Buffer** buf,
                    int* size,
                    bool record)
    {
        if (!queue_array_peek(queue))
            return NULL;

        BufferLL* container;
        Buffer *ret;

        // lock this
        pthread_mutex_lock(&queue->mutex);
        {
            container = queue->first;
            ret = container->obj;
            queue->first = container->next;
        }
        pthread_mutex_unlock(&queue->mutex);

        free(container);
        *buf = ret;
        pointer data;
        if (record)
        {
#ifndef MINGW
        data = util::object_class_init()->ref(ret,size,"\\screencoder_queue.cpp",__LINE__);
        util::object_class_init()->unref(ret,"\\screencoder_queue.cpp",__LINE__);
#else
        data = util::object_class_init()->ref(ret,size,"/screencoder_queue.cpp",__LINE__);
        util::object_class_init()->unref(ret,"/screencoder_queue.cpp",__LINE__);
#endif
        } else {
            data = util::object_class_init()->ref(ret,size,"/util/",__LINE__);
            util::object_class_init()->unref(ret,"/util/",__LINE__);
        }
        queue->size--;
        return data;
    }


    QueueArray*     
    queue_array_init()
    {
        QueueArray* array = (QueueArray*)malloc(sizeof(QueueArray));
        array->first = NULL;
        array->size = 0;
        array->mutex = PTHREAD_MUTEX_INITIALIZER;
        return array;
    }


    void            
    queue_array_finalize(QueueArray* queue)
    {
        while(queue_array_peek(queue))
        {
            util::Buffer* buf;
            queue_array_pop(queue,&buf,NULL,false);
#ifndef MINGW
        util::object_class_init()->unref(buf,"\\screencoder_queue.cpp",__LINE__);
#else
        util::object_class_init()->unref(buf,"/screencoder_queue.cpp",__LINE__);
#endif
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