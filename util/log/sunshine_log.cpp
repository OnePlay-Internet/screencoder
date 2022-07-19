/**
 * @file sunshine_log.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_log.h>
#include <cstdio>

namespace error
{
    void log(char* file,
             int line,
             char* level,
             char* message)
    {
        printf("%s : %d : %s : %s\n",file,line,level,message);
        return;
    }
    
} // namespace error
