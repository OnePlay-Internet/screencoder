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

#include <screencoder_datatype.h>
#include <string>
#include <typeinfo>

#define BUFFER_TRACE

#define BUFFER_CLASS         util::object_class_init() 

#ifdef BUFFER_TRACE
#define BUFFER_REF(obj,size)                        util::object_class_init()->ref(obj,size,__FILE__,__LINE__)

#define BUFFER_UNREF(obj)                           util::object_class_init()->unref(obj,__FILE__,__LINE__)

#define BUFFER_INIT(obj,size,free)                  util::object_class_init()->init(obj,size,free,__FILE__,__LINE__,(char*)typeid(obj).name())
#else
#define BUFFER_REF(obj,size)                        util::object_class_init()->ref(obj,size)

#define BUFFER_UNREF(obj)                           util::object_class_init()->unref(obj)

#define BUFFER_INIT(obj,size,free)                  util::object_class_init()->init(obj,size,free)
#endif





/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data pointer
 */
#define BUFFER_MALLOC(x,y,z) pointer z = (pointer)malloc( y );  \
                             memset(z,0,y); \
                             util::Buffer* x = BUFFER_INIT(z,y,free) 


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data source
 */
#define BUFFER_DUPLICATE(x,y,z,ptr) pointer ptr = (pointer)malloc( y );  \
                                    memcpy(ptr,z,y); \
                                    util::Buffer* x = BUFFER_INIT(ptr,y,free) \

namespace util 
{
    typedef void (*BufferFreeFunc) (pointer data);

    typedef struct _Buffer Buffer;

    typedef struct _BufferLL BufferLL;

    struct _BufferLL {
        Buffer* obj;

        BufferLL* next;
    };

    typedef void (*InsertAction) (util::Buffer* buf,
                                  int index,
                                  int index_total);

    typedef struct _BufferClass {
        pointer (*ref)      (Buffer* obj,
                             int* size
    #ifdef BUFFER_TRACE
                             ,char* file
                             ,int line
    #endif
                             );

        void    (*unref)    (Buffer* obj
    #ifdef BUFFER_TRACE
                             ,char* file
                             ,int line
    #endif
                             );

        Buffer* (*init)     (pointer data,
                             uint size,
                             BufferFreeFunc func
    #ifdef BUFFER_TRACE
                             ,char* file
                             ,int line
                             ,char* type
    #endif
                             );

        Buffer* (*duplicate)(Buffer* obj);

        uint    (*size)     (Buffer* obj);
    } BufferClass;


    
    BufferClass*        object_class_init       ();
} // namespace queue



#endif