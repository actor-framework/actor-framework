/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_THREAD_HPP
#define CAF_THREAD_HPP

#ifdef __RIOTBUILD_FLAG

#include <tuple>
#include <chrono>
#include <memory>
#include <utility>
#include <exception>
#include <functional>
#include <type_traits>
//#include <system_error>
#include <stdexcept>

extern "C" {
#include <time.h>
#include "thread.h"
}

#include "mutex.hpp"
#include "condition_variable.hpp"
#include "thread_util.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"

namespace caf {

namespace {
  constexpr kernel_pid_t thread_uninitialized = -1;
  constexpr size_t stack_size = KERNEL_CONF_STACKSIZE_MAIN;
}

class thread_id {
  template <class T,class Traits>
  friend std::basic_ostream<T,Traits>&
    operator<<(std::basic_ostream<T,Traits>& out, thread_id id);
  friend class thread;

 public:
  inline thread_id() noexcept : m_handle{thread_uninitialized} { }
  inline thread_id(kernel_pid_t handle) : m_handle{handle} {}

  inline bool operator==(thread_id other) noexcept {
    return m_handle == other.m_handle;
  }
  inline bool operator!=(thread_id other) noexcept {
    return !(m_handle == other.m_handle);
  }
  inline bool operator< (thread_id other) noexcept {
    return m_handle < other.m_handle;
  }
  inline bool operator<=(thread_id other) noexcept {
    return !(m_handle > other.m_handle);
  }
  inline bool operator> (thread_id other) noexcept {
    return m_handle > other.m_handle;
  }
  inline bool operator>=(thread_id other) noexcept {
    return !(m_handle < other.m_handle);
  }

 private:
  kernel_pid_t m_handle;
};

template <class T,class Traits>
inline std::basic_ostream<T,Traits>&
  operator<<(std::basic_ostream<T,Traits>& out, thread_id id) {
  return out << id.m_handle;
}

namespace this_thread {

  inline thread_id get_id() noexcept { return thread_getpid(); }
  inline void yield() noexcept { thread_yield(); }
  void sleep_for(const std::chrono::nanoseconds& ns);
  template <class Rep, class Period>
  void sleep_for(const std::chrono::duration<Rep, Period>& sleep_duration) {
    using namespace std::chrono;
    if (sleep_duration > std::chrono::duration<Rep, Period>::zero()) {
      constexpr std::chrono::duration<long double> max = nanoseconds::max();
      nanoseconds ns;
      if (sleep_duration < max) {
        ns = duration_cast<nanoseconds>(sleep_duration);
        if (ns < sleep_duration) {
          ++ns;
        }
      }
      else {
        ns = nanoseconds::max();
      }
      sleep_for(ns);
    }
  }
  template <class Clock, class Period>
  void sleep_until(const std::chrono::time_point<Clock, Period>& sleep_time) {
    using namespace std::chrono;
    mutex mtx;
    condition_variable cv;
    unique_lock<mutex> lk(mtx);
    while(Clock::now() < sleep_time) {
      cv.wait_until(lk,sleep_time);
    }
  }
  template <class Period>
  inline void sleep_until(const std::chrono::time_point<std::chrono::steady_clock,
                                                        Period>& sleep_time) {
      sleep_for(sleep_time - std::chrono::steady_clock::now());
  }
} // namespace this_thread

class thread {

 public:
  using id = thread_id;
  using native_handle_type = kernel_pid_t;

  inline thread() noexcept : m_handle{thread_uninitialized} { }
  template <class F, class ...Args,
            class = typename std::enable_if<
              !std::is_same<typename std::decay<F>::type,thread>::value
            >::type
           >
  explicit thread(F&& f, Args&&... args);
  ~thread();

  thread(const thread&) = delete;
  inline thread(thread&& t) noexcept : m_handle{t.m_handle} {
    t.m_handle = thread_uninitialized;
    std::swap(m_stack, t.m_stack);
  }
  thread& operator=(const thread&) = delete;
  thread& operator=(thread&&) noexcept;

  void swap(thread& t) noexcept {
    std::swap(m_handle, t.m_handle);
    std::swap(m_stack, t.m_stack);
  }

  inline bool joinable() const noexcept { return false; }
  void join();
  void detach();
  inline id get_id() const noexcept { return m_handle; }
  inline native_handle_type native_handle() noexcept { return m_handle; }

  static unsigned hardware_concurrency() noexcept;

  kernel_pid_t m_handle;
  std::unique_ptr<char[]> m_stack;
};

void swap(thread& lhs, thread& rhs) noexcept;

template <class F>
void* thread_proxy(void* vp) {
  std::unique_ptr<F> p(static_cast<F*>(vp));
  auto indices =
    detail::get_right_indices<caf::detail::tl_size<F>::value - 1>(*p);
  detail::apply_args(std::get<0>(*p), indices, *p);
  return nullptr;
}

template <class F, class ...Args,
         class
        >
thread::thread(F&& f, Args&&... args) : m_stack(new char[stack_size]) {
  using namespace std;
  using func_and_args = tuple<typename decay<F>::type,
                              typename decay<Args>::type...>;
//  if(!m_stack) {
//    m_stack = std::unique_ptr<char[]>(new char[stack_size]);
//  }
  std::unique_ptr<func_and_args> p(new func_and_args(decay_copy(forward<F>(f)),
                                     decay_copy(forward<Args>(args))...));
  m_handle = thread_create(m_stack.get(), stack_size,
                           PRIORITY_MAIN - 1, 0, // CREATE_WOUT_YIELD
                           &thread_proxy<func_and_args>,
                           p.get(), "caf_thread");
  if (m_handle >= 0) {
    p.release();
  } else {
    throw std::runtime_error("Failed to create thread.");
  }
}

inline thread& thread::operator=(thread&& other) noexcept {
  if (m_handle != thread_uninitialized || m_stack) {
    std::terminate();
  }
  m_handle = other.m_handle;
  other.m_handle = thread_uninitialized;
  std::swap(m_stack, other.m_stack);
  return *this;
}

inline void swap (thread& lhs, thread& rhs) noexcept {
  lhs.swap(rhs);
}

} // namespace caf

#else

#include <time.h>

#include <thread>

namespace caf {

using thread = std::thread;

namespace this_thread {

using std::this_thread::get_id;

// GCC hack
#if !defined(_GLIBCXX_USE_SCHED_YIELD) && !defined(__clang__)
inline void yield() noexcept {
  timespec req;
  req.tv_sec = 0;
  req.tv_nsec = 1;
  nanosleep(&req, nullptr);
}
#else
using std::this_thread::yield;
#endif

// another GCC hack
#if !defined(_GLIBCXX_USE_NANOSLEEP) && !defined(__clang__)
template <class Rep, typename Period>
inline void sleep_for(const chrono::duration<Rep, Period>& rt) {
  auto sec = chrono::duration_cast<chrono::seconds>(rt);
  auto nsec = chrono::duration_cast<chrono::nanoseconds>(rt - sec);
  timespec req;
  req.tv_sec = sec.count();
  req.tv_nsec = nsec.count();
  nanosleep(&req, nullptr);
}
#else
using std::this_thread::sleep_for;
#endif

} // namespace this_thread

} // namespace caf

#endif

#endif // CAF_THREAD_HPP
