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



typedef struct  _ArrayObject         ArrayObject;      




/**
 * @brief 
 * 
 * @return ArrayObject* 
 */
ArrayObject*    array_object_new           ();

/**
 * @brief 
 * 
 * @return ArrayObject* 
 */
void            array_object_finalize           (ArrayObject* arr);
/**
 * @brief 
 * 
 * @param array 
 * @param data 
 */
void            array_object_emplace_back (ArrayObject* array,
                                           pointer data);

/**
 * @brief 
 * 
 * @param array 
 * @param data 
 */
pointer         array_object_get_data     (ArrayObject* array,
                                           int index);

bool            array_object_has_data     (ArrayObject* array,
                                           int index);

int             array_object_length       (ArrayObject* array);

#endif