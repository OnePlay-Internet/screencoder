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


#ifndef __SUNSHINE_OBJECT_H__
#define __SUNSHINE_OBJECT_H__

#include <sunshine_datatype.h>

#define OBJECT_CLASS       object::object_class_init() 

namespace object
{
    typedef void (*ObjectFreeFunc) (pointer data);

    typedef struct _Object{
        uint refcount;
        ObjectFreeFunc free_func;
        pointer data;
    } Object;

    typedef struct _ObjectClass {
        pointer (*ref)      (Object* obj);

        void    (*unref)    (Object* obj);

        Object* (*init)     (pointer data,ObjectFreeFunc func);
    } ObjectClass;

    
    ObjectClass*        object_class_init       ();
} // namespace queue



#endif