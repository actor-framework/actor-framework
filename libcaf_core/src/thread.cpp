
#include <time.h>

#include <cerrno>

#include "caf/thread.hpp"

namespace caf {

thread::~thread() {
  // not needed, as our thread is always detachted
  if (m_stack && m_handle != thread_uninitialized) {
    //sched_task_exit();
    //dINT();
    //sched_threads[sched_active_pid] = NULL;
    --sched_num_threads;
    //sched_set_status((tcb_t *)sched_active_thread, STATUS_STOPPED);
    //sched_active_thread = NULL;
    //cpu_switch_context_exit();
  }
}

void thread::join() {
  // you can't join threads
}

void thread::detach() {
  // I believe there is no equivalent on RIOT
  if (m_stack) {
    m_stack.release();
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
