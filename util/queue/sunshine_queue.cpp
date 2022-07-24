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
#include <sunshine_queue.h>
#include <sunshine_macro.h>
#include <sunshine_datatype.h>
#include <sunshine_object.h>
#include <cstdlib>
#include <mutex>
#include <string.h>

#define BASE_SIZE 1024

using namespace std;

namespace util {
    /**
     * @brief 
     * Circular Array
     */
    struct _QueueArray {
        std::mutex _lock;

        /**
         * @brief 
         * 
         */
        BufferLL* first;
    };


    bool            queue_array_push        (QueueArray* queue, 
                                             Buffer* object);

    bool            queue_array_peek        (QueueArray* queue);


    pointer         queue_array_pop         (QueueArray* queue, 
                                             util::Buffer** buf,
                                             int* size);

                                             
    QueueArray*     queue_array_init        ();

    void            queue_array_finalize    (QueueArray* queue);



    /**
     * @brief 
     * 
     */
    QueueArrayClass*
    queue_array_class_init()
    {
        static bool initialized = false;
        static QueueArrayClass klass = {0};
        if (initialized)
            return &klass;

        klass.init = queue_array_init;
        klass.peek = queue_array_peek;
        klass.pop  = queue_array_pop;
        klass.push = queue_array_push;
        klass.stop = queue_array_finalize;
        initialized = true;
        return &klass;
    }

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
        std::lock_guard lg (queue->_lock);
        BufferLL* last = (BufferLL*)malloc(sizeof(BufferLL));
        memset(last,0,sizeof(BufferLL));

        BUFFER_CLASS->ref(obj,NULL);
        last->obj  = obj;
        last->next = NULL;

        if(!queue->first)
        {
            queue->first = last;
        }
        else
        {
            BufferLL* container = queue->first;
            while (container->next) { container = container->next; }
            container->next = last;
        }

        return true;
    }


    bool            
    queue_array_peek(QueueArray* queue)
    {
        return queue->first ? true : false;
    }


    pointer
    queue_array_pop(QueueArray* queue, 
                    util::Buffer** buf,
                    int* size)
    {
        std::lock_guard lg (queue->_lock);
        if (!queue_array_peek(queue))
            return NULL;

        BufferLL* container = queue->first;
        Buffer *ret = container->obj;


        queue->first = container->next;
        free(container);
        return BUFFER_CLASS->ref(ret,size);
    }


    QueueArray*     
    queue_array_init()
    {
        QueueArray* array = (QueueArray*)malloc(sizeof(QueueArray));
        memset(array,0,sizeof(QueueArray));

        array->first = NULL;
        return array;
    }


    void            
    queue_array_finalize(QueueArray* queue)
    {
        queue->_lock.unlock();
        free(queue);
    }
}