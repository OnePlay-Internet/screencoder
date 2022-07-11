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
#include <sunshine_log.h>
#include <sunshine_macro.h>
#include <sunshine_error.h>
#include <sunshine_datatype.h>
#include <sunshine_object.h>
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
        /**
         * @brief 
         * 
         */
        uint length;

        std::mutex _lock;

        /**
         * @brief 
         * 
         */
        ObjectContainer* first;
    };


    bool            queue_array_push        (QueueArray* queue, 
                                             Object* object);

    bool            queue_array_peek        (QueueArray* queue);

    Object*          queue_array_pop         (QueueArray* queue);

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
                     util::Object* obj)
    {
        std::lock_guard {queue->_lock};
        ObjectContainer* container = queue->first;
        while (!container->next) { container = container->next; }
        
        ObjectContainer* last = (ObjectContainer*)malloc(sizeof(ObjectContainer));
        memset(last,0,sizeof(ObjectContainer));

        last->obj  = obj;
        last->next = NULL;

        container->next = last;
        queue->length++;
        return true;
    }


    bool            
    queue_array_peek(QueueArray* queue)
    {
        std::lock_guard {queue->_lock};
        if (queue->length > 0)
            return false;
        else
            return true;
    }


    util::Object* 
    queue_array_pop(QueueArray* queue)
    {
        std::lock_guard {queue->_lock};
        if (!queue_array_peek(queue))
            return NULL;

        ObjectContainer* container = queue->first;
        Object *ret = container->obj;
        queue->first = container->next;
        free(container);
        queue->length--;
        return ret;
    }


    QueueArray*     
    queue_array_init()
    {
        QueueArray* array = (QueueArray*)malloc(sizeof(QueueArray));
        memset(array,0,sizeof(QueueArray));

        array->length = 0;
        array->first = NULL;
        return array;
    }


    void            
    queue_array_finalize(QueueArray* queue)
    {
        std::lock_guard {queue->_lock};
        free(queue);
    }
}