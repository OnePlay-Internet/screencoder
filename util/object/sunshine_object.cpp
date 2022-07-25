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
#include <sunshine_macro.h>
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
    };



    Buffer*
    object_duplicate (Buffer* obj)
    {
        Buffer* object = (Buffer*)malloc(sizeof(Buffer));
        memset(object,0,sizeof(Buffer));

        memcpy(object->data,obj->data,obj->size);
        memcpy(object,obj,sizeof(Buffer));
        object->ref_count = 1;
        return object;
    }

    pointer 
    object_ref (Buffer* obj,
                int* size)
    {
        obj->ref_count++;
        if (size)
            *size = obj->size;
        
        return obj->data;
    }

    Buffer*
    buffer_merge(Buffer* buffer,
                 Buffer* inserter)
    {
        uint new_size = buffer->size+inserter->size;
        pointer new_ptr = malloc(new_size);
        memcpy(new_ptr,buffer,buffer->size);
        memcpy(new_ptr+buffer->size,inserter,inserter->size);
        return BUFFER_CLASS->init(new_ptr,new_size,free);
    }

    uint
    object_size (Buffer* obj)
    {
        return obj->size;
    }

    void    
    object_unref (Buffer* obj)
    {
        obj->ref_count--;
        if (!obj->ref_count)
        {
            obj->free_func(obj->data);
            free(obj);
        }
    }


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

        byte* next = (byte*)data->data;
        for(int x = 0; x < elements; ++x) {
            pointer p = ptr + (x * (insert_size + slice_size));

            Buffer* buf = BUFFER_CLASS->init(p,insert_size,DO_NOTHING);
            action(buf,x,elements);
            BUFFER_CLASS->unref(buf);

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

            memcpy(p + insert_size,next, slice_size);
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
        byte* string_ptr = (byte*)BUFFER_CLASS->ref(string,&size_string);
        byte* substring_ptr = (byte*)BUFFER_CLASS->ref(substring,&size_substring);

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
        BUFFER_CLASS->unref(string);
        BUFFER_CLASS->unref(substring);
        return ret;
    }
    
    util::Buffer*
    replace(util::Buffer* original, 
            util::Buffer* old, 
            util::Buffer* _new) 
    {
        int size_origin, size_old, size_new;
        byte* origin_ptr = (byte*)BUFFER_CLASS->ref(original,&size_origin);
        byte* old_ptr = (byte*)BUFFER_CLASS->ref(old,&size_old);
        byte* new_ptr = (byte*)BUFFER_CLASS->ref(_new,&size_new);

        uint replace_size = size_origin - size_old + MAX(size_old,size_new);
        BUFFER_MALLOC(ret,replace_size,replaced);

        uint inserter = search(original,old);
        memcpy(replaced,original,inserter);
        uint origin_found = inserter;

        if(inserter != size_origin) {
            memcpy(replaced + inserter, _new, size_new);
            inserter += size_new;
            memcpy(replaced + inserter, original + size_old + origin_found, size_new);
        }

        BUFFER_CLASS->unref(original);
        BUFFER_CLASS->unref(_new);
        BUFFER_CLASS->unref(old);
        return ret;
    }



    BufferClass*
    object_class_init()
    {
        static bool initialized = false;
        static BufferClass klass = {0};
        if (initialized)
            return &klass;

        klass.init  = object_init;
        klass.unref = object_unref;
        klass.ref   = object_ref;
        klass.duplicate = object_duplicate;
        klass.size  = object_size;
        klass.merge = buffer_merge;
        klass.replace = replace;
        klass.insert  = insert;
        klass.search = search;
        initialized = true;
        return &klass;
    }
} // namespace object