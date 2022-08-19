/**
 * @file screencoder_log.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <screencoder_log.h>
#include <screencoder_util.h>
#include <cstdio>

#include <thread>

#define LOG_QUEUE error::get_log_queue()


using namespace std::literals;

namespace error
{
    typedef struct _Err {
        int line;
        char file[100];
        char level[100];
        char message[100];
    }Err;

    void
    render_log(util::QueueArray* array) 
    {
        while (QUEUE_ARRAY_CLASS->peek(array))
        {
            util::Buffer* buf;
            Err* err = (Err*)QUEUE_ARRAY_CLASS->pop(array,&buf,NULL);


            BUFFER_UNREF(buf);
            std::this_thread::sleep_for(10ms);
        }
    }

    util::QueueArray*
    get_log_queue()
    {
        util::QueueArray* ret;
        RETURN_ONCE(ret);
        ret = QUEUE_ARRAY_CLASS->init();
        std::thread{render_log,ret};
    }



    void log(char* file,
             int line,
             char* level,
             char* message)
    {
        BUFFER_MALLOC(buf,sizeof(Err),ptr);
        memcpy(((Err*)ptr)->message,message,strlen(message));
        memcpy(((Err*)ptr)->level,level,strlen(level));
        memcpy(((Err*)ptr)->file,file,strlen(file));
        ((Err*)ptr)->line = line;
        QUEUE_ARRAY_CLASS->push(LOG_QUEUE,buf);
        return;
    }
    
        // snprintf((char*)ptr, 1000, "%s : %d : %s : %s\n",file,line,level,message);
} // namespace error
