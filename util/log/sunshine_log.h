/**
 * @file sunshine_error.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __SUNSHINE_LOG_H__
#define __SUNSHINE_LOG_H__



#define LOG_ERROR(content)  error::new_error(__FILE__,__LINE__,"error",content);


#endif