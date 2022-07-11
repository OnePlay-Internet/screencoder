/**
 * @file object.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <sunshine_object.h>
#include <sunshine_datatype.h>
#include <cstdlib>
#include <string.h>
#include <mutex>

namespace util 
{

    pointer object_ref (Object* obj);
    void    object_unref (Object* obj);
    Object* object_init (pointer data, ObjectFreeFunc free);



    pointer 
    object_ref (Object* obj)
    {
        obj->ref_count++;
        return obj->data;
    }

    pointer 
    object_end_pointer (Object* obj)
    {
        return ((byte*)obj->data) + obj->size;
    }

    
    uint
    object_size (Object* obj)
    {
        return obj->size;
    }

    void    
    object_unref (Object* obj)
    {
        obj->ref_count--;
        if (!obj->ref_count)
        {
            obj->free_func(obj->data);
            free(obj);
        }
    }

    Object* 
    object_init (pointer data, 
                 uint size,
                 ObjectFreeFunc free_func)
    {
        Object* object = (Object*)malloc(sizeof(Object));
        memset(object,0,sizeof(Object));

        object->data = data;
        object->free_func = free_func;
        object->size = size,
        object->ref_count = 1;
        return object;
    }




    ObjectClass*
    object_class_init()
    {
        static bool initialized = false;
        static ObjectClass klass = {0};
        if (initialized)
            return &klass;

        klass.init  = object_init;
        klass.unref = object_unref;
        klass.ref   = object_ref;
        klass.size  = object_size;
        initialized = true;
        return &klass;
    }
} // namespace object