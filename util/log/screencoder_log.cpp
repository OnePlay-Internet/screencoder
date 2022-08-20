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
        char time[100];
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

            char file_log[36] = {" "};
            file_log[35] = 0;
            snprintf(file_log,35,"FILE: %s:%d",find_from_back(err->file,"/"),err->line);

            for (int i = 0; i < 36; i++) {
                if (file_log[i] == 0) {
                    memcpy(file_log + i ," ",1);
                }
            }

            char level_log[31] = {" "};
            level_log[30] = 0;
            snprintf(level_log,30,"LEVEL: %s",err->level);

            for (int i = 0; i < 31; i++) {
                if (level_log[i] == 0) {
                    memcpy(level_log + i ," ",1);
                }
            }

            char time_log[61] = {" "};
            time_log[60] = 0;
            snprintf(time_log,60,"TIMESTAMP: %s",err->time);

            for (int i = 0; i < 61; i++) {
                if (time_log[i] == 0) {
                    memcpy(time_log+ i ," ",1);
                }
            }
              

            outfile << level_log << "||"<< file_log << "||"<< time_log << " || MESSAGE: "<< err->message << std::endl; 

            if(find_substr(err->level,"trace"))
                goto done;

            std::cout << " LOG LEVEL: " << err->level << " || FILE : " << find_from_back(err->file,"/") << " : " << err->line << " || MESSAGE: "<< err->message << "\n"; 
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


    void
    get_string_fmt(std::chrono::nanoseconds time,
                   char** string)
    {
        auto min = std::to_string(std::chrono::duration_cast<std::chrono::minutes>(time).count() %1000 %1000 %1000 %1000 %60).data();
        auto sec = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(time).count() %1000 %1000 % 1000 % 1000).data();
        auto mili = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(time).count() %1000 % 1000 % 1000).data();
        auto micro = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(time).count() %1000 % 1000).data();
        auto nano = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(time).count() %1000 ).data();
        snprintf(*string,100,"(min:%s|sec:%s|mili:%s|micro:%s|nano:%s)",min,sec,mili,micro,nano);
    }

    std::chrono::nanoseconds 
    get_start_time()
    {
        static std::chrono::high_resolution_clock::time_point fix;
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        RETURN_ONCE(now - fix);
        fix = std::chrono::high_resolution_clock::now();
        return now - fix;
    }

    void log(char* file,
             int line,
             char* level,
             char* message)
    {
        if(find_substr(file,"/util/"))
            return;

        char timestr[100] = {0};
        char* temp = timestr;
        auto time = get_start_time();
        get_string_fmt(time,&temp);

        BUFFER_MALLOC(buf,sizeof(Err),ptr);
        memcpy(((Err*)ptr)->message,message,strlen(message));
        memcpy(((Err*)ptr)->level,level,strlen(level));
        memcpy(((Err*)ptr)->file,file,strlen(file));
        memcpy(((Err*)ptr)->time,timestr,strlen(timestr));
        ((Err*)ptr)->line = line;
        QUEUE_ARRAY_CLASS->push(LOG_QUEUE,buf);
        BUFFER_UNREF(buf);
        return;
    }

    char*
    map_event(BufferEventType type)
    {
        switch (type)
        {
        case BufferEventType::INIT :
            return "INIT";
        case BufferEventType::REF :
            return "REF";
        case BufferEventType::UNREF :
            return "UNREF";
        case BufferEventType::FREE:
            return "FREE";
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

        char timestampbuf[100] = {0}; char* timestampStr = timestampbuf;
        get_string_fmt(timestampnano,&timestampStr);

        auto createTime = std::chrono::duration_cast<std::chrono::milliseconds>(log->created.time_since_epoch());
        auto strTemp = std::to_string(createTime.count());
        char* createTimeStr = strTemp.substr(strTemp.length()-3,3).data();
        

        char str[100] = {0};
        snprintf(str, 100, "buffer id %s contain %s : %s at %s",createTimeStr,log->dataType,map_event(type),timestampStr);
        error::log(file,line,"trace",str);
    }
} // namespace error
