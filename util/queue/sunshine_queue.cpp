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
        uint capacity;
        uint length;

        /**
         * @brief 
         * 
         */
        uint head;
        uint tail;

        std::mutex _lock;

        /**
         * @brief 
         * 
         */
        object::Object* object_array;
    };


    bool            queue_array_push        (QueueArray* queue, object::Object object);
    bool            queue_array_peek        (QueueArray* queue);
    void            queue_array_pop         (QueueArray* queue, object::Object object);
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
        klass.init = queue_array_init;
        initialized = true;
        return &klass;
    }


    static void
    queue_array_do_expand(QueueArray* array)
    {
        // calculate new size of queue array
        uint newsize;
        int oldsize = array->capacity;
        /* newsize is 50% bigger */
        newsize = MAX ((3 * (int) oldsize) / 2, (int) oldsize + 1);


        if (newsize > UINT_MAX)
            LOG_ERROR("growing the queue array would overflow");


        pointer new_buffer = malloc(newsize);
        memset(new_buffer,0,newsize);

        pointer head_pointer = (pointer)((uint)( array->head * sizeof(object::Object)) + (uint)(array->object_array));
        pointer tail_pointer = (pointer)((uint)( array->tail * sizeof(object::Object)) + (uint)(array->object_array));

        pointer new_head_pointer = (pointer)(new_buffer);
        pointer new_tail_pointer = (pointer)((uint)( array->capacity * sizeof(object::Object)) + (uint)(new_buffer));

        uint copy_size;
        if (array->head < array->tail)
        {
            copy_size = (sizeof(object::Object) * (uint)(oldsize));
            memcpy(new_head_pointer,head_pointer,copy_size);
        }
        else if (array->head > array->tail)
        {
            copy_size = (sizeof(object::Object) * (uint)(oldsize - array->head));
            memcpy(new_head_pointer,head_pointer,copy_size);
            pointer new_boundary = (pointer)(copy_size  + (uint)new_head_pointer);
            copy_size = (sizeof(object::Object) * (uint)(array->tail));
            memcpy(new_boundary,array->object_array,copy_size);
        }
        else 
        {
            // do_nothing
        }

        array->object_array = (object::Object*)new_head_pointer;
        array->head = 0;
        array->tail = oldsize;
        array->capacity = newsize;
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
                     object::Object* obj)
    {
        std::lock_guard {queue->_lock};
        if (queue->length == (queue->capacity - 1))
            queue_array_do_expand(queue);
        

        queue->tail++;
        if (queue->tail == queue->capacity)
        {
            if (queue->head > 0)
                queue->tail = 0;
            else
                LOG_ERROR("invalid queue");
        }

        object::Object* asserted_obj = (object::Object*) (queue->object_array + ( sizeof(object::Object) * (uint) queue->tail));
        OBJECT_CLASS->ref(asserted_obj);
        asserted_obj->data = obj->data;
        queue->length++;
    }


    bool            
    queue_array_peek(QueueArray* queue)
    {
        std::lock_guard {queue->_lock};
        if (queue->length > 0)
        {
            return false;
        }

        pointer head_pointer = (pointer)(queue->object_array + ( sizeof(object::Object) * (uint) queue->head));
        if (!head_pointer)
        {
            LOG_ERROR("invalid queue");
            return false;
        }
    }


    void
    queue_array_pop(QueueArray* queue, 
                    object::Object* obj)
    {
        std::lock_guard {queue->_lock};
        if (queue_array_peek(queue))
        {
            object::Object* head_pointer = (object::Object*)(queue->object_array + ( sizeof(object::Object) * (uint) queue->head));
            obj->data = head_pointer->data;
        }
        OBJECT_CLASS->unref(obj);
    }


    QueueArray*     
    queue_array_init()
    {
        QueueArray* array = (QueueArray*)malloc(sizeof(QueueArray));
        memset(array,0,sizeof(QueueArray));
        array->head = 0;
        array->tail = 1;
        array->length = 0;
        array->capacity = BASE_SIZE;
        array->object_array = (object::Object*)malloc((uint)BASE_SIZE * (uint)sizeof(object::Object));
        return array;
    }


    void            
    queue_array_finalize(QueueArray* queue)
    {
        std::lock_guard {queue->_lock};
        free(queue->object_array);
        free(queue);
    }
}