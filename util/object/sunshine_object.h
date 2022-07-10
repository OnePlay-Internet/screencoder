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

#define OBJECT_HOLDER(x)     util::Object* x = NULL;


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data pointer
 */
#define OBJECT_MALLOC(x,y,z) OBJECT_HOLDER(x); \
                             pointer z = (pointer)malloc( y );  \
                             memset(z,0,y); \
                             OBJECT_CLASS->init(x,z,y,free) 


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data pointer
 */
#define OBJECT_DUPLICATE(x,y,z) OBJECT_HOLDER(z); \
                                pointer ptr = (pointer)malloc( y );  \
                                memset(ptr,0,y); \
                                memcpy(ptr,x,y); \
                                OBJECT_CLASS->init(z,ptr,y,free) \

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

    typedef struct _ObjectContainer {
        Object* obj;

        Object* next;
    }ObjectContainer;

    typedef struct _ObjectClass {
        pointer (*ref)      (Object* obj);

        pointer (*end_ptr)  (Object* obj);

        void    (*unref)    (Object* obj);

        void    (*init)     (Object* obj, 
                             pointer data,
                             uint size,
                             ObjectFreeFunc func);
        
        uint    (*size)     (Object* obj);
    } ObjectClass;

    
    ObjectClass*        object_class_init       ();
} // namespace queue



#endif