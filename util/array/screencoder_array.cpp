/**
 * @file screencoder_array.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_array.h>
#include <screencoder_macro.h>
#include <string.h>



namespace util
{
    /**
     * @brief 
     * 
     */
    struct _ListObject{
        BufferLL* first;

        uint length;
    };      


    ListObject*
    list_object_new()
    {
        ListObject* arr = (ListObject*)malloc(sizeof(ListObject));
        memset(arr,0,sizeof(ListObject));

        arr->length=0;
        return arr;
    }

    void
    array_object_finalize(ListObject* arr)
    {
        BufferLL* container = arr->first;
        while (!container->next) 
        { 
            BUFFER_CLASS->unref(container->obj);
            container = container->next; 
        }
        free(arr);
    }

    void
    array_object_emplace_back(ListObject* array,
                              Buffer* obj)
    {
        BufferLL* container = NULL;
        if (array->length)
        {
            container = array->first;
            while (!container->next) { container = container->next; }
        }
        
        BufferLL* last = (BufferLL*)malloc(sizeof(BufferLL));
        memset(last,0,sizeof(BufferLL));

        last->obj  = obj;
        last->next = NULL;

        if (array->length)
            container->next = last;
        else
            array->first = last;

        array->length++;
    }

    bool
    array_object_has_data(ListObject* array,
                          int index)
    {
        return index < array->length;
    }

    Buffer* 
    array_object_get_data(ListObject* array,
                          int index)
    {
        if (!array_object_has_data(array,index))
            return NULL;
        
        int count = 0;
        BufferLL* prev_container,*container,*next_container;
        container = array->first;
        while (!container->next || count == index) 
        { 
            prev_container = container;
            container = container->next; 

            if (container->next)
                next_container = container->next;
        }

        Buffer* ret = container->obj;

        if(next_container)
            prev_container->next = next_container;

        return ret;
    }


    int             
    array_object_length(ListObject* array)
    {
        return array->length;
    }
    
    ListObjectClass* 
    list_object_class_init()
    {
        static bool initialize = false;
        static ListObjectClass klass = {0};
        if (initialize)
            return &klass;
        
        initialize = true;
        klass.emplace_back = array_object_emplace_back;
        klass.finalize     = array_object_finalize;
        klass.has_data     = array_object_has_data;
        klass.get_data     = array_object_get_data;
        klass.init         = list_object_new;
        klass.length       = array_object_length;
        return &klass;
    }



    KeyValue* 
    new_keyvalue_pairs(int size)
    {
        KeyValue* pairs = (KeyValue*)malloc((size + 1) * sizeof(KeyValue));
        memset(pairs,0,(size + 1)  * sizeof(KeyValue));
        return pairs;
    }

    void
    keyval_new_strval(KeyValue* pair, 
                      char* key, 
                      char* val)
    {
        int i = 0;
        while ((pair + i)->type)
        {
            (pair + i)->type = Type::INT;
            (pair + i)->key  = key;
            (pair + i)->string_value = val;
            i++;
        }
    }

    void
    keyval_new_intval(KeyValue* pair, 
                      char* key, 
                      int val)
    {
        int i = 0;
        while ((pair + i)->type)
        {
            (pair + i)->type = Type::INT;




            (pair + i)->key = key;





            (pair + i)->int_value = val;
            i++;
        }
    }
} // namespace util

