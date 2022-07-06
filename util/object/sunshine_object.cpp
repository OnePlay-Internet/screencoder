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

namespace object
{
    pointer object_ref (Object* obj);
    void    object_unref (Object* obj);
    Object* object_init (pointer data);

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
        initialized = true;
        return &klass;
    }

    pointer 
    object_ref (Object* obj)
    {
        obj->ref_count++;
        return obj->data;
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
    object_init (pointer data, ObjectFreeFunc free_func)
    {
        Object* obj = (Object*) malloc(sizeof(Object));
        memset(obj,0,sizeof(Object));
        obj->data = data;
        obj->free_func = free_func;
        obj->ref_count  = 1;
        return obj;
    }
} // namespace object