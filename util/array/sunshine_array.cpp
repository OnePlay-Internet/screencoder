/**
 * @file sunshine_array.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_array.h>
#include <sunshine_macro.h>




#define MAX_LENGTH      1024

/**
 * @brief 
 * 
 */
struct _ArrayObject{
    object::Object* array[MAX_LENGTH];
    int length;
};      


ArrayObject*
array_object_new()
{
    ArrayObject* arr = malloc(sizeof(ArrayObject));
    memset(arr,0,sizeof(ArrayObject))
    arr->length=0;
    return arr;
}

void
array_object_finalize(ArrayObject* arr)
{
    // OBJECT_CLASS->unref()
}

void
array_object_emplace_back(ArrayObject* array,
                          pointer data)
{
    int i = 0;
    while (array->array[i])
    {
        i++;
    }

    array->array[i] = OBJECT_CLASS->init(data,DO_NOTHING)
    array->length++;
}

pointer         
array_object_get_data(ArrayObject* array,
                      int index)
{
    return array->array[index]->data;
}

bool
array_object_has_data(ArrayObject* array,
                      int index)
{
    return index <= array->length;
}