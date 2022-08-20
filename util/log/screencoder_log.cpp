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

    char*
    map_event(BufferEventType type)
    {
        switch (type)
        {
        case BufferEventType::INIT :
            return "init";
        case BufferEventType::REF :
            return "ref";
        case BufferEventType::UNREF :
            return "unref";
        case BufferEventType::FREE:
            return "free";
        default:
            return "unknown";
        }
    }

    void log_buffer(BufferLog* log,
                    int line,
                    char* file,
                    BufferEventType type)
    {

        auto timestamp = std::chrono::high_resolution_clock::now() - log->created;
        auto timestampnano = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp);
        char* timestampStr = std::to_string(timestampnano.count()).data();

        auto createTime = std::chrono::duration_cast<std::chrono::milliseconds>(log->created.time_since_epoch());
        auto strTemp = std::to_string(createTime.count());
        char* createTimeStr = strTemp.substr(strTemp.length()-3,3).data();
        

        char str[100] = {0};
        snprintf(str, 100, "buffer id %s contain %s : %s at %s",createTimeStr,log->dataType,map_event(type),timestampStr);
        error::log(file,line,"trace",str);
    }
} // namespace error
