
#include <time.h>

#include <cerrno>

#include "caf/thread.hpp"

namespace caf {

thread::~thread() {
  // not needed, as our thread is always detachted
  if (m_handle != thread_uninitialized) {
    throw std::runtime_error("Thread is not joined or detached!");
  }
}

void thread::join() {
  if (m_handle != thread_uninitialized) {
    m_data->joining_thread = sched_active_pid;
    m_handle = thread_uninitialized;
    thread_sleep();
  }
}

void thread::join() {
  // you can't join threads
}

void thread::detach() {
  if (m_handle != thread_uninitialized) {
    m_handle = thread_uninitialized;
  }
}

unsigned thread::hardware_concurrency() noexcept {
  // embedded systems often do not have more cores
  // RIOT does currently not ofter these informations
  return 1;
}

namespace this_thread {

void sleep_for(const std::chrono::nanoseconds& ns) {
  using namespace std::chrono;
  if (ns > nanoseconds::zero()) {
    seconds s = duration_cast<seconds>(ns);
    timespec ts;
    using ts_sec = decltype(ts.tv_sec);
    // typedef decltype(ts.tv_sec) ts_sec;
    constexpr ts_sec ts_sec_max = std::numeric_limits<ts_sec>::max();
    if (s.count() < ts_sec_max) {
        ts.tv_sec = static_cast<ts_sec>(s.count());
        ts.tv_nsec = static_cast<decltype(ts.tv_nsec)>((ns-s).count());
    } else {
      ts.tv_sec = ts_sec_max;
      ts.tv_nsec = std::giga::num - 1;
    }
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { ; }
  }
}

} // namespace this_thread

} // namespace caf
