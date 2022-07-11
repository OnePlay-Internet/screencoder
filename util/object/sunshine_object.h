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
#include <string>

#define OBJECT_CLASS         util::object_class_init() 


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data pointer
 */
#define OBJECT_MALLOC(x,y,z) pointer z = (pointer)malloc( y );  \
                             memset(z,0,y); \
                             util::Object* x = OBJECT_CLASS->init(z,y,free) 


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data source
 */
#define OBJECT_DUPLICATE(x,y,z,ptr) pointer ptr = (pointer)malloc( y );  \
                                    memcpy(ptr,z,y); \
                                    util::Object* x = OBJECT_CLASS->init(ptr,y,free) \

namespace util 
{
    typedef void (*ObjectFreeFunc) (pointer data);

    typedef struct _Object{
        uint ref_count;

        ObjectFreeFunc free_func;

        uint size;

        /**
         * @brief 
         * should not be used directly
         */
        pointer data;
    }Object;

    typedef struct _ObjectContainer ObjectContainer;

    struct _ObjectContainer {
        Object* obj;

        ObjectContainer* next;
    };

    typedef struct _ObjectClass {
        pointer (*ref)      (Object* obj);

        pointer (*end_ptr)  (Object* obj);

        void    (*unref)    (Object* obj);

        Object* (*init)     (pointer data,
                             uint size,
                             ObjectFreeFunc func);
        
        uint    (*size)     (Object* obj);
    } ObjectClass;

    
    ObjectClass*        object_class_init       ();
} // namespace queue



#endif