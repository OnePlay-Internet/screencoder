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

#include <screencoder_object.h>
#include <screencoder_datatype.h>
#include <screencoder_macro.h>
#include <cstdlib>
#include <string.h>
#include <mutex>



namespace util 
{


    typedef struct _Buffer{
        uint ref_count;

        /**
         * @brief 
         * should not be used directly
         */
        BufferFreeFunc free_func;

        /**
         * @brief 
         * should not be used directly
         */
        uint size;

        /**
         * @brief 
         * should not be used directly
         */
        pointer data;

        /**
         * @brief 
         * 
         */
        std::chrono::high_resolution_clock::time_point created;
    #ifdef BUFFER_TRACE

        error::BufferLog log;
    #endif
    };



    Buffer*
    object_duplicate (Buffer* obj)
    {
        Buffer* object = (Buffer*)malloc(sizeof(Buffer));
        memset(object,0,sizeof(Buffer));
        memcpy(object,obj,sizeof(Buffer));

        object->data = malloc(obj->size);
        memcpy(object->data,obj->data,obj->size);

        object->ref_count = 1;
        object->created = std::chrono::high_resolution_clock::now();
        return object;
    }

#ifndef BUFFER_TRACE
    Buffer* 
    object_init (pointer data, 
                 uint size,
                 BufferFreeFunc free_func)
    {
        Buffer* object = (Buffer*)malloc(sizeof(Buffer));
        memset(object,0,sizeof(Buffer));

        object->data = data;
        object->free_func = free_func;
        object->size = size,
        object->ref_count = 1;
        return object;
    }
    pointer 
    object_ref (Buffer* obj,
                int* size)
    {

        if(FILTER_ERROR(obj))
            return;

        obj->ref_count++;
        if (size)
            *size = obj->size;
        
        return obj->data;
    }

    void    
    object_unref (Buffer* obj)
    {
        if(FILTER_ERROR(obj))
            return;

        obj->ref_count--;
        if (!obj->ref_count)
        {
            obj->free_func(obj->data);
            free(obj);
        }
    }
#else
    Buffer* 
    object_init (pointer data, 
                 uint size,
                 BufferFreeFunc free_func,
                 char* file,
                 int line,
                 char* type)
    {
        Buffer* object = (Buffer*)malloc(sizeof(Buffer));
        memset(object,0,sizeof(Buffer));
        memcpy(object->log.dataType,type,strlen(type));
        object->created = std::chrono::high_resolution_clock::now();

        object->data = data;
        object->free_func = free_func;
        object->size = size,
        object->ref_count = 1;


        error::log_buffer(&object->log,object->created,line,file,error::BufferEventType::INIT);

        return object;
    }
    pointer 
    object_ref (Buffer* obj,
                int* size,
                char* file,
                int line)
    {
        error::log_buffer(&obj->log,obj->created,line,file,error::BufferEventType::REF);

        obj->ref_count++;
        if (size)
            *size = obj->size;
        
        return obj->data;
    }

    void    
    object_unref (Buffer* obj,
                 char* file,
                 int line)
    {
        error::log_buffer(&obj->log,obj->created,line,file,error::BufferEventType::UNREF);
        
        obj->ref_count--;
        if (!obj->ref_count) {
            error::log_buffer(&obj->log,obj->created,line,file,error::BufferEventType::FREE);
            obj->free_func(obj->data);
            free(obj);
        }
    }

#endif

    Buffer*
    buffer_merge(Buffer* buffer,
                 Buffer* inserter)
    {
        uint new_size = buffer->size+inserter->size;
        pointer new_ptr = malloc(new_size);
        memcpy(new_ptr,buffer,buffer->size);
        memcpy((char*)new_ptr+buffer->size,inserter,inserter->size);
        return BUFFER_INIT(new_ptr,new_size,free);
    }

    uint
    object_size (Buffer* obj)
    {
        return obj->size;
    }

    int64
    object_create(Buffer* obj)
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(obj->created.time_since_epoch()).count();
    }





    /**
     * @brief 
     * |---------------------buffer--------------------------------|
     * |----------|----------|----------|----------|----------|----|
     * |<--slice->|
     * |---|----------|---|----------|---|----------|---|----------|---|----------|---|----|
     * |<->| <-- insert                  |<--slice->|                                                                 
     * |-----------------------------------return_buffer-----------------------------------|
     * @param insert_size 
     * @param slice_size 
     * @param data 
     * @param action 
     * @return char* 
     */
    util::Buffer*
    insert(uint64 insert_size, 
           uint64 slice_size, 
           util::Buffer* data,
           InsertAction action) 
    {
        int pad     =  data->size % (int)slice_size ;
        int elements = data->size / slice_size + ((pad != 0) ? 1 : 0);

        BUFFER_MALLOC(ret,elements * insert_size + data->size,ptr);

        uint8* next = (uint8*)data->data;
        for(int x = 0; x < elements; ++x) {
            pointer p = (char*)ptr + (x * (insert_size + slice_size));

            Buffer* buf = BUFFER_INIT(p,insert_size,DO_NOTHING);
            action(buf,x,elements);
            BUFFER_UNREF(buf);

            /**
             * @brief 
             * copy slice from source to destination
             * p + insertsize
             */
            int copy_size;
            if (x == (elements - 1) && pad != 0)
                copy_size = pad;
            else
                copy_size = slice_size;

            memcpy((char*)p + insert_size,next, slice_size);
            next += slice_size;
        }
        return ret;
    }


    /**
     * @brief 
     * return character position which substring start inside string
     * @param string 
     * @param substring 
     * @return uint 
     */
    uint
    search(util::Buffer* string, 
           util::Buffer* substring)
    {
        int size_string, size_substring;
        uint8* string_ptr = (uint8*)BUFFER_REF(string,&size_string);
        uint8* substring_ptr = (uint8*)BUFFER_REF(substring,&size_substring);

        int ret = 1;
        while (ret < size_string)
        {
            int i = 1;
            while (*(substring_ptr + i) == *(string_ptr + ret + i))
            {
                if (i = size_substring)
                    return ret;

                i++;
            }
            ret++;
        }
        BUFFER_UNREF(string);
        BUFFER_UNREF(substring);
        return ret;
    }
    
    util::Buffer*
    replace(util::Buffer* original, 
            util::Buffer* old, 
            util::Buffer* _new) 
    {
        int size_origin, size_old, size_new;
        uint8* origin_ptr = (uint8*)BUFFER_REF(original,&size_origin);
        uint8* old_ptr = (uint8*)BUFFER_REF(old,&size_old);
        uint8* new_ptr = (uint8*)BUFFER_REF(_new,&size_new);

        uint replace_size = size_origin - size_old + MAX(size_old,size_new);
        BUFFER_MALLOC(ret,replace_size,replaced);

        uint inserter = search(original,old);
        memcpy(replaced,original,inserter);
        uint origin_found = inserter;

        if(inserter != size_origin) {
            memcpy((char*)replaced + inserter, _new, size_new);
            inserter += size_new;
            memcpy((char*)replaced + inserter, original + size_old + origin_found, size_new);
        }

        BUFFER_UNREF(original);
        BUFFER_UNREF(_new);
        BUFFER_UNREF(old);
        return ret;
    }



    BufferClass*
    object_class_init()
    {
        static BufferClass klass = {0};
        RETURN_PTR_ONCE(klass);

        klass.init  = object_init;
        klass.unref = object_unref;
        klass.ref   = object_ref;
        klass.duplicate = object_duplicate;
        klass.size  = object_size;
        klass.created = object_create;
        return &klass;
    }
} // namespace object