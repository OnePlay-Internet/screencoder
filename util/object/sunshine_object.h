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

#define BUFFER_CLASS         util::object_class_init() 


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data pointer
 */
#define BUFFER_MALLOC(x,y,z) pointer z = (pointer)malloc( y );  \
                             memset(z,0,y); \
                             util::Buffer* x = BUFFER_CLASS->init(z,y,free) 


/**
 * @brief 
 * x: object name
 * y: object data size
 * z: object data source
 */
#define BUFFER_DUPLICATE(x,y,z,ptr) pointer ptr = (pointer)malloc( y );  \
                                    memcpy(ptr,z,y); \
                                    util::Buffer* x = BUFFER_CLASS->init(ptr,y,free) \

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
                             int* size);

        Buffer* (*duplicate)(Buffer* obj);

        Buffer* (*merge)   (Buffer* self,
                             Buffer* inserter);

        Buffer* (*replace)  (util::Buffer* original, 
                            util::Buffer* old, 
                            util::Buffer* _new) ;

        Buffer* (*insert)   (uint64 insert_size, 
                             uint64 slice_size, 
                             util::Buffer* data,
                             InsertAction action);

        uint    (*search)   (util::Buffer* string, 
                             util::Buffer* substring);

        void    (*unref)    (Buffer* obj);

        void    (*lock)     (Buffer* obj);

        Buffer* (*init)     (pointer data,
                             uint size,
                             BufferFreeFunc func);
        
        uint    (*size)     (Buffer* obj);
    } BufferClass;


    
    BufferClass*        object_class_init       ();
} // namespace queue



#endif