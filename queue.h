#ifndef SUNSHINE_THREAD_SAFE_H
#define SUNSHINE_THREAD_SAFE_H



#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

template<class T>
class queue_t {
public:
  using status_t = util::optional_t<T>;

  queue_t(std::uint32_t max_elements = 32) : _max_elements { max_elements } {}

  template<class... Args>
  void raise(Args &&...args) {
    std::lock_guard ul { _lock };

    if(!_continue) {
      return;
    }

    if(_queue.size() == _max_elements) {
      _queue.clear();
    }

    _queue.emplace_back(std::forward<Args>(args)...);

    _cv.notify_all();
  }

  bool peek() {
    return _continue && !_queue.empty();
  }

  template<class Rep, class Period>
  status_t pop(std::chrono::duration<Rep, Period> delay) {
    std::unique_lock ul { _lock };

    if(!_continue) {
      return util::false_v<status_t>;
    }

    while(_queue.empty()) {
      if(!_continue || _cv.wait_for(ul, delay) == std::cv_status::timeout) {
        return util::false_v<status_t>;
      }
    }

    auto val = std::move(_queue.front());
    _queue.erase(std::begin(_queue));

    return val;
  }

  status_t pop() {
    std::unique_lock ul { _lock };

    if(!_continue) {
      return util::false_v<status_t>;
    }

    while(_queue.empty()) {
      _cv.wait(ul);

      if(!_continue) {
        return util::false_v<status_t>;
      }
    }

    auto val = std::move(_queue.front());
    _queue.erase(std::begin(_queue));

    return val;
  }

  std::vector<T> &unsafe() {
    return _queue;
  }

  void stop() {
    std::lock_guard lg { _lock };

    _continue = false;

    _cv.notify_all();
  }

  [[nodiscard]] bool running() const {
    return _continue;
  }

private:
  bool _continue { true };
  std::uint32_t _max_elements;

  std::mutex _lock;
  std::condition_variable _cv;

  std::vector<T> _queue;
};


#endif