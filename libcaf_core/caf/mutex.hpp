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

#ifndef CAF_MUTEX_HPP
#define CAF_MUTEX_HPP

#ifdef __RIOTBUILD_FLAG

#include <utility>
//#include <system_error>
#include <stdexcept>

#include <cstdio>

extern "C" {
#include "mutex.h"
}

namespace caf {

class mutex {

 public:
  using native_handle_type = mutex_t*;

  inline constexpr mutex() noexcept : m_mtx{0, PRIORITY_QUEUE_INIT} { }
  ~mutex();

  void lock();
  bool try_lock() noexcept;
  void unlock() noexcept;

  inline native_handle_type native_handle() { return &m_mtx; }

 private:
  mutex(const mutex&);
  mutex& operator=(const mutex&);

  mutex_t m_mtx;
};

struct defer_lock_t {};
struct try_to_lock_t {};
struct adopt_lock_t {};

constexpr defer_lock_t  defer_lock  = defer_lock_t();
constexpr try_to_lock_t try_to_lock = try_to_lock_t();
constexpr adopt_lock_t  adopt_lock  = adopt_lock_t();

template<class Mutex>
class lock_guard {

 public:
  using mutex_type = Mutex;

  inline explicit lock_guard(mutex_type& mtx) : m_mtx{mtx} { m_mtx.lock(); }
  inline lock_guard(mutex_type& mtx, adopt_lock_t) : m_mtx{mtx} {}
  inline ~lock_guard() { m_mtx.unlock(); }

 private:
  mutex_type& m_mtx;
};

template<class Mutex>
class unique_lock {

 public:
  using mutex_type = Mutex;

  inline unique_lock() noexcept : m_mtx{nullptr}, m_owns{false} {}
  inline explicit unique_lock(mutex_type& mtx) : m_mtx{&mtx}, m_owns{true} {
    m_mtx->lock();
  }
  inline unique_lock(mutex_type& mtx, defer_lock_t) noexcept
    : m_mtx{&mtx}, m_owns{false} { }
  inline unique_lock(mutex_type& mtx, try_to_lock_t)
    : m_mtx{&mtx}, m_owns{ mtx.try_lock() } { }
  inline unique_lock(mutex_type& mtx, adopt_lock_t)
    : m_mtx{&mtx}, m_owns{true} { }
  template<class Clock, class Duration>
//  inline unique_lock(mutex_type& mtx,
//                     const std::chrono::time_point<Clock,
//                                                   Duration>& timeout_time)
//    : m_mtx{&mtx}, m_owns{mtx.try_lock_until(timeout_time)} { }
//  template<class Rep, class Period>
//  inline unique_lock(mutex_type& mtx,
//                     const chrono::duration<Rep,Period>& timeout_duration)
//    : m_mtx{&mtx}, m_owns{mtx.try_lock_for(timeout_duration)} { }
//  inline ~unique_lock() {
//    if (m_owns) {
//      m_mtx->unlock();
//    }
//  }
  inline unique_lock(unique_lock&& lock) noexcept
    : m_mtx{lock.m_mtx},
      m_owns{lock.m_owns} {
    lock.m_mtx = nullptr;
    lock.m_owns = false;
  }
  inline unique_lock& operator=(unique_lock&& lock) noexcept {
    if (m_owns) {
      m_mtx->unlock();
    }
    m_mtx = lock.m_mtx;
    m_owns = lock.m_owns;
    lock.m_mtx = nullptr;
    lock.m_owns = false;
    return *this;
  }

  void lock();
  bool try_lock();

//  template <class Rep, class Period>
//    bool try_lock_for(const chrono::duration<Rep, Period>& timeout_duration);
//  template <class Clock, class Duration>
//    bool try_lock_until(const chrono::time_point<Clock,Duration>& timeout_time);

  void unlock();

  inline void swap(unique_lock& lock) noexcept {
    std::swap(m_mtx, lock.m_mtx);
    std::swap(m_owns, lock.m_owns);
  }

  inline mutex_type* release() noexcept {
    mutex_type* mtx = m_mtx;
    m_mtx = nullptr;
    m_owns = false;
    return mtx;
  }

  inline bool owns_lock() const noexcept { return m_owns; }
  inline explicit operator bool () const noexcept { return m_owns; }
  inline mutex_type* mutex() const noexcept { return m_mtx; }

 private:
  unique_lock(unique_lock const&);
  unique_lock& operator=(unique_lock const&);

  mutex_type* m_mtx;
  bool m_owns;
};

template<class Mutex>
void unique_lock<Mutex>::lock() {
  if (m_mtx == nullptr) {
    throw std::runtime_error("unique_lock::lock: references null mutex");
  }
  if (m_owns) {
    throw std::runtime_error("");
  }
  m_mtx->lock();
  m_owns = true;
}

template<class Mutex>
bool unique_lock<Mutex>::try_lock() {
  if (m_mtx == nullptr) {
    throw std::runtime_error("unique_lock::try_lock: references null mutex");
  }
  if (m_owns) {
    throw std::runtime_error("unique_lock::try_lock lock: already locked");
  }
  m_owns = m_mtx->try_lock();
  return m_owns;
}

//template<class Mutex>
//template<class Rep, class Period>
//bool unique_lock<Mutex>
//  ::try_lock_for(const std::chrono::duration<Rep,Period>& timeout_duration) {
//  if (m_mtx == nullptr) {
//    throw std::runtime_error("unique_lock::try_lock_for: references null mutex");
//  }
//  if (m_owns) {
//    throw std::runtime_error("unique_lock::try_lock_for: already locked");
//  }
//  m_owns = m_mtx->try_lock_until(timeout_duration);
//  return m_owns;
//}

//template <class Mutex>
//template <class Clock, class Duration>
//bool unique_lock<Mutex>
//  ::try_lock_until(const chrono::time_point<Clock, Duration>& timeout_time) {
//    if (m_mtx == nullptr)
//        throw std::runtime_error("unique_lock::try_lock_until: "
//                                    "references null mutex");
//    if (m_owns)
//        throw std::runtime_error("unique_lock::try_lock_until: "
//                                      "already locked");
//    m_owns = m_mtx->try_lock_until(timeout_time);
//    return m_owns;
//}

template<class Mutex>
void unique_lock<Mutex>::unlock() {
  if (!m_owns) {
    throw std::runtime_error("unique_lock::unlock: not locked");
  }
  m_mtx->unlock();
  m_owns = false;
}

template<class Mutex>
inline void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) noexcept {
  lhs.swap(rhs);
}

} // namespace caf

#else

#include <mutex>

namespace caf {

using mutex = std::mutex;
template <class Mutex> using lock_guard = std::lock_guard<Mutex>;
template <class Mutex> using unique_lock = std::unique_lock<Mutex>;

} // namespace caf

#endif

#endif // CAF_MUTEX_HPP
