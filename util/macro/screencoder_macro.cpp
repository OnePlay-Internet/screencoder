/**
 * @file screencoder_macro.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_macro.h>
#include <string.h>


void do_nothing(void*){}

bool 
string_compare(char* a, 
               char* b)
{
    if (!a || !b)
        return false;
    
    int i = 0;
    if (strlen(a) != strlen(b))
        return false;

    while (*(a+i) && *(a + i) == *(b + i))
    {
        i++;
    }

    if (strlen(a) == i)
        return true;  
    else
        return false;
}
