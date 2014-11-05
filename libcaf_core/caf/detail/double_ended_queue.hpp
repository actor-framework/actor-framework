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
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_DOUBLE_ENDED_QUEUE_HPP
#define CAF_DETAIL_DOUBLE_ENDED_QUEUE_HPP

#include "caf/config.hpp"

#define CAF_CACHE_LINE_SIZE 64

#include <chrono>
#include <thread>
#include <atomic>
#include <cassert>

// GCC hack
#if defined(CAF_GCC) && !defined(_GLIBCXX_USE_SCHED_YIELD)
#include <time.h>
namespace std {
namespace this_thread {
namespace {
inline void yield() noexcept {
  timespec req;
  req.tv_sec = 0;
  req.tv_nsec = 1;
  nanosleep(&req, nullptr);
}
} // namespace anonymous>
} // namespace this_thread
} // namespace std
#endif

// another GCC hack
#if defined(CAF_GCC) && !defined(_GLIBCXX_USE_NANOSLEEP)
#include <time.h>
namespace std {
namespace this_thread {
namespace {
template <class Rep, typename Period>
inline void sleep_for(const chrono::duration<Rep, Period>& rt) {
  auto sec = chrono::duration_cast<chrono::seconds>(rt);
  auto nsec = chrono::duration_cast<chrono::nanoseconds>(rt - sec);
  timespec req;
  req.tv_sec = sec.count();
  req.tv_nsec = nsec.count();
  nanosleep(&req, nullptr);
}
} // namespace anonymous>
} // namespace this_thread
} // namespace std
#endif

namespace caf {
namespace detail {

/*
 * A thread-safe double-ended queue based on http://drdobbs.com/cpp/211601363.
 * This implementation is optimized for FIFO, i.e., it supports fast insertion
 * at the end and fast removal from the beginning. As long as the queue is
 * only used for FIFO operations, readers do not block writers and vice versa.
 */
template <class T>
class double_ended_queue {
 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  class node {
   public:
    pointer value;
    std::atomic<node*> next;
    node(pointer val) : value(val), next(nullptr) {
      // nop
    }
   private:
    static constexpr size_type payload_size =
      sizeof(pointer) + sizeof(std::atomic<node*>);
    static constexpr size_type cline_size = CAF_CACHE_LINE_SIZE;
    static constexpr size_type pad_size =
      (cline_size * ((payload_size / cline_size) + 1)) - payload_size;
    // avoid false sharing
    static_assert(pad_size > 0, "invalid padding size calculated");
    char pad[pad_size];
  };

  using unique_node_ptr = std::unique_ptr<node>;

  static_assert(sizeof(node*) < CAF_CACHE_LINE_SIZE,
                "sizeof(node*) >= CAF_CACHE_LINE_SIZE");

  double_ended_queue() {
    m_head_lock.clear();
    m_tail_lock.clear();
    auto ptr = new node(nullptr);
    m_head = ptr;
    m_tail = ptr;
  }

  ~double_ended_queue() {
    auto ptr = m_head.load();
    while (ptr) {
      unique_node_ptr tmp{ptr};
      ptr = tmp->next.load();
    }
  }

  // acquires only one lock
  void append(pointer value) {
    CAF_REQUIRE(value != nullptr);
    node* tmp = new node(value);
    lock_guard guard(m_tail_lock);
    // publish & swing last forward
    m_tail.load()->next = tmp;
    m_tail = tmp;
  }

  // acquires both locks
  void prepend(pointer value) {
    CAF_REQUIRE(value != nullptr);
    node* tmp = new node(value);
    node* first = nullptr;
    // acquire both locks since we might touch m_last too
    lock_guard guard1(m_head_lock);
    lock_guard guard2(m_tail_lock);
    first = m_head.load();
    CAF_REQUIRE(first != nullptr);
    auto next = first->next.load();
    // m_first always points to a dummy with no value,
    // hence we put the new element second
    if (next == nullptr) {
      // queue is empty
      CAF_REQUIRE(first == m_tail);
      m_tail = tmp;
    } else {
      CAF_REQUIRE(first != m_tail);
      tmp->next = next;
    }
    first->next = tmp;
  }

  // acquires only one lock, returns nullptr on failure
  pointer take_head() {
    unique_node_ptr first;
    pointer result = nullptr;
    { // lifetime scope of guard
      lock_guard guard(m_head_lock);
      first.reset(m_head.load());
      node* next = m_head.load()->next;
      if (next == nullptr) {
        // queue is empty
        first.release();
        return nullptr;
      }
      // take it out of the node & swing first forward
      result = next->value;
      next->value = nullptr;
      m_head = next;
    }
    return result;
  }

  // acquires both locks, returns nullptr on failure
  pointer take_tail() {
    pointer result = nullptr;
    unique_node_ptr last;
    { // lifetime scope of guards
      lock_guard guard1(m_head_lock);
      lock_guard guard2(m_tail_lock);
      CAF_REQUIRE(m_head != nullptr);
      last.reset(m_tail.load());
      if (last.get() == m_head.load()) {
        last.release();
        return nullptr;
      }
      result = last->value;
      m_tail = find_predecessor(last.get());
      CAF_REQUIRE(m_tail != nullptr);
      m_tail.load()->next = nullptr;
    }
    return result;
  }

  // does not lock
  bool empty() const {
    // atomically compares first and last pointer without locks
    return m_head == m_tail;
  }

 private:
  // precondition: *both* locks acquired
  node* find_predecessor(node* what) {
    for (auto i = m_head.load(); i != nullptr; i = i->next) {
      if (i->next == what) {
        return i;
      }
    }
    return nullptr;
  }

  // guarded by m_head_lock
  std::atomic<node*> m_head;
  char m_pad1[CAF_CACHE_LINE_SIZE - sizeof(node*)];
  // guarded by m_tail_lock
  std::atomic<node*> m_tail;
  char m_pad2[CAF_CACHE_LINE_SIZE - sizeof(node*)];
  // enforce exclusive access
  std::atomic_flag m_head_lock;
  std::atomic_flag m_tail_lock;

  class lock_guard {
   public:
    lock_guard(std::atomic_flag& lock) : m_lock(lock) {
      while (lock.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
    }
    ~lock_guard() {
      m_lock.clear(std::memory_order_release);
    }
   private:
    std::atomic_flag& m_lock;
  };
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_DOUBLE_ENDED_QUEUE_HPP
