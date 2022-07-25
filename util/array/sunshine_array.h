/**
 * @file sunshine_array.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_ARRAY_H__
#define __SUNSHINE_ARRAY_H__


#include <sunshine_datatype.h>
#include <sunshine_object.h>

#define LIST_OBJECT_CLASS       util::list_object_class_init()


namespace util
{
    typedef struct  _ListObject         ListObject;      

    typedef struct _ListObjectClass {
        ListObject*     (*init)             ();

        void            (*finalize)         (ListObject* list);

        void            (*emplace_back)     (ListObject* list,
                                             Buffer* obj);

        bool            (*has_data)         (ListObject* list,
                                             int index);

        Buffer*         (*get_data)         (ListObject* list,  
                                             int index);

        int             (*length)           (ListObject*);
    }ListObjectClass;

    ListObjectClass* list_object_class_init    ();

    typedef enum _Type
    {
        NONE,
        STRING,
        INT,
    }Type;


    typedef struct _KeyValue {
        char* key;
        Type type;
        char* string_value;
        int int_value;
    }KeyValue;

    KeyValue* new_keyvalue_pairs (int size);

    void
    keyval_new_intval(KeyValue* pair, 
                      char* key, 
                      int val);
    

    void
    keyval_new_strval(KeyValue* pair, 
                      char* key, 
                      char* val);
}
#endif