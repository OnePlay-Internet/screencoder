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
#include <fstream>
#include <iostream>

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

    static char*
    find_from_back(char* str, char* word)
    {
        int count = strlen(str) - 1;
        while (*(str+count) != *word) {
            count--;
        }
        return str + count + 1;
    }

    static bool
    find_substr(char* str, char* word)
    {
        int count = 0;
        while (*(str+count)) {
            
            int y = 0;
            while (y+count < strlen(str) && *(str+count + y) == *(word+y)) {
                if (strlen(word) == y + 1) {
                    return true;
                }
                y++;
            }
            count++;
        }

        return false;
    }

    void
    render_log(util::QueueArray* array) 
    {
        std::ofstream outfile;
        outfile.open("./log.txt", std::ios_base::app); // append instead of overwrite
        while(true)
        {
            if(!QUEUE_ARRAY_CLASS->peek(array)) {
                std::this_thread::sleep_for(10ms);
                continue;
            }

            util::Buffer* buf;
            Err* err = (Err*)QUEUE_ARRAY_CLASS->pop(array,&buf,NULL);

            outfile << " LOG LEVEL: " << err->level << " || FILE : " << find_from_back(err->file,"/") << " : " << err->line << " || MESSAGE: "<< err->message << "\n"; 

            if(find_substr(err->level,"buffer"))
                goto done;

            std::cout << " LOG LEVEL: " << err->level << " || FILE : " << find_from_back(err->file,"/") << " : " << err->line << " || MESSAGE: "<< err->message << std::endl; 
        done:
            BUFFER_UNREF(buf);
        }
    }

    util::QueueArray*
    get_log_queue()
    {
        static util::QueueArray* ret;
        RETURN_ONCE(ret);
        ret = QUEUE_ARRAY_CLASS->init();
        std::thread render {render_log,ret};
        render.detach();
        return ret;
    }



    void log(char* file,
             int line,
             char* level,
             char* message)
    {
        if(find_substr(file,"/util/"))
            return;

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
