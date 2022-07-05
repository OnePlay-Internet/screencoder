#ifndef SUNSHINE_SINGLETON_H
#define SUNSHINE_SINGLETON_H

#include <singleton.h>
#include <thread_safe.h>



namespace singleton {
  class video_capturer
  {
  private:
    util::ThreadPool task_pool;
    safe::mail_t man;
    bool display_cursor;
  public:
    video_capturer (/* args */);
    ~video_capturer ();

    template<T>
    auto get_mail(mail::mail_id id);

  };  
}

singleton::video_capturer capturer;




#endif