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

#ifndef CAF_SINGLE_READER_QUEUE_HPP
#define CAF_SINGLE_READER_QUEUE_HPP

#include <list>
#include <mutex>
#include <atomic>
#include <memory>
#include <limits>
#include <condition_variable> // std::cv_status

#include "caf/config.hpp"

namespace caf {
namespace detail {

/**
 * Denotes in which state queue and reader are after an enqueue.
 */
enum class enqueue_result {
  /**
   * Indicates that the enqueue operation succeeded and
   * the reader is ready to receive the data.
   */
  success,

  /**
   * Indicates that the enqueue operation succeeded and
   * the reader is currently blocked, i.e., needs to be re-scheduled.
   */
  unblocked_reader,

  /**
   * Indicates that the enqueue operation failed because the
   * queue has been closed by the reader.
   */
  queue_closed
};

/**
 * An intrusive, thread-safe queue implementation.
 */
template <class T, class Delete = std::default_delete<T>>
class single_reader_queue {
 public:
  using value_type = T;
  using pointer = value_type*;

  /**
   * Tries to dequeue a new element from the mailbox.
   * @warning Call only from the reader (owner).
   */
  pointer try_pop() {
    return take_head();
  }

  /**
   * Tries to enqueue a new element to the mailbox.
   * @warning Call only from the reader (owner).
   */
  enqueue_result enqueue(pointer new_element) {
    CAF_REQUIRE(new_element != nullptr);
    pointer e = m_stack.load();
    for (;;) {
      if (!e) {
        // if tail is nullptr, the queue has been closed
        m_delete(new_element);
        return enqueue_result::queue_closed;
      }
      // a dummy is never part of a non-empty list
      new_element->next = is_dummy(e) ? nullptr : e;
      if (m_stack.compare_exchange_strong(e, new_element)) {
        return  (e == reader_blocked_dummy()) ? enqueue_result::unblocked_reader
                                              : enqueue_result::success;
      }
      // continue with new value of e
    }
  }

  /**
   * Queries whether there is new data to read, i.e., whether the next
   * call to {@link try_pop} would succeeed.
   * @pre !closed()
   */
  bool can_fetch_more() {
    if (m_head != nullptr) {
      return true;
    }
    auto ptr = m_stack.load();
    CAF_REQUIRE(ptr != nullptr);
    return !is_dummy(ptr);
  }

  /**
   * Queries whether this queue is empty.
   * @warning Call only from the reader (owner).
   */
  bool empty() {
    CAF_REQUIRE(!closed());
    return m_head == nullptr && is_dummy(m_stack.load());
  }

  /**
   * Queries whether this has been closed.
   */
  bool closed() {
    return m_stack.load() == nullptr;
  }

  /**
   * Queries whether this has been marked as blocked, i.e.,
   * the owner of the list is waiting for new data.
   */
  bool blocked() {
    return m_stack.load() == reader_blocked_dummy();
  }

  /**
   * Tries to set this queue from state `empty` to state `blocked`.
   */
  bool try_block() {
    auto e = stack_empty_dummy();
    bool res = m_stack.compare_exchange_strong(e, reader_blocked_dummy());
    CAF_REQUIRE(e != nullptr);
    // return true in case queue was already blocked
    return res || e == reader_blocked_dummy();
  }

  /**
   * Tries to set this queue from state `blocked` to state `empty`.
   */
  bool try_unblock() {
    auto e = reader_blocked_dummy();
    return m_stack.compare_exchange_strong(e, stack_empty_dummy());
  }

  /**
   * Closes this queue and deletes all remaining elements.
   * @warning Call only from the reader (owner).
   */
  void close() {
    clear_cached_elements();
    if (fetch_new_data(nullptr)) {
      clear_cached_elements();
    }
  }

  /**
   * Closes this queue and applies f to all remaining
   *        elements before deleting them.
   * @warning Call only from the reader (owner).
   */
  template <class F>
  void close(const F& f) {
    clear_cached_elements(f);
    if (fetch_new_data(nullptr)) {
      clear_cached_elements(f);
    }
  }

  single_reader_queue() : m_head(nullptr) {
    m_stack = stack_empty_dummy();
  }

  void clear() {
    if (!closed()) {
      clear_cached_elements();
      if (fetch_new_data()) {
        clear_cached_elements();
      }
    }
  }

  ~single_reader_queue() {
    clear();
  }

  size_t count(size_t max_count = std::numeric_limits<size_t>::max()) {
    size_t res = 0;
    fetch_new_data();
    auto ptr = m_head;
    while (ptr && res < max_count) {
      ptr = ptr->next;
      ++res;
    }
    return res;
  }

  /**************************************************************************
   *                    support for synchronized access                     *
   **************************************************************************/

  template <class Mutex, class CondVar>
  bool synchronized_enqueue(Mutex& mtx, CondVar& cv, pointer new_element) {
    switch (enqueue(new_element)) {
      case enqueue_result::unblocked_reader: {
        std::unique_lock<Mutex> guard(mtx);
        cv.notify_one();
        return true;
      }
      case enqueue_result::success:
        // enqueued message to a running actor's mailbox
        return true;
      case enqueue_result::queue_closed:
        // actor no longer alive
        return false;
    }
    // should be unreachable
    CAF_CRITICAL("invalid result of enqueue()");
  }

  template <class Mutex, class CondVar>
  void synchronized_await(Mutex& mtx, CondVar& cv) {
    CAF_REQUIRE(!closed());
    if (!can_fetch_more() && try_block()) {
      std::unique_lock<Mutex> guard(mtx);
      while (blocked()) {
        cv.wait(guard);
      }
    }
  }

  template <class Mutex, class CondVar, class TimePoint>
  bool synchronized_await(Mutex& mtx, CondVar& cv, const TimePoint& timeout) {
    CAF_REQUIRE(!closed());
    if (!can_fetch_more() && try_block()) {
      std::unique_lock<Mutex> guard(mtx);
      while (blocked()) {
        if (cv.wait_until(guard, timeout) == std::cv_status::timeout) {
          // if we're unable to set the queue from blocked to empty,
          // than there's a new element in the list
          return !try_unblock();
        }
      }
    }
    return true;
  }

 private:
  // exposed to "outside" access
  std::atomic<pointer> m_stack;

  // accessed only by the owner
  pointer m_head;
  Delete m_delete;

  // atomically sets m_stack back and enqueues all elements to the cache
  bool fetch_new_data(pointer end_ptr) {
    CAF_REQUIRE(end_ptr == nullptr || end_ptr == stack_empty_dummy());
    pointer e = m_stack.load();
    // must not be called on a closed queue
    CAF_REQUIRE(e != nullptr);
    // fetching data while blocked is an error
    CAF_REQUIRE(e != reader_blocked_dummy());
    // it's enough to check this once, since only the owner is allowed
    // to close the queue and only the owner is allowed to call this
    // member function
    while (e != end_ptr) {
      if (m_stack.compare_exchange_weak(e, end_ptr)) {
        // fetching data while blocked is an error
        CAF_REQUIRE(e != reader_blocked_dummy());
        if (is_dummy(e)) {
          // only use-case for this is closing a queue
          CAF_REQUIRE(end_ptr == nullptr);
          return false;
        }
        while (e) {
          CAF_REQUIRE(!is_dummy(e));
          auto next = e->next;
          e->next = m_head;
          m_head = e;
          e = next;
        }
        return true;
      }
      // next iteration
    }
    return false;
  }

  bool fetch_new_data() {
    return fetch_new_data(stack_empty_dummy());
  }

  pointer take_head() {
    if (m_head != nullptr || fetch_new_data()) {
      auto result = m_head;
      m_head = m_head->next;
      return result;
    }
    return nullptr;
  }

  void clear_cached_elements() {
    while (m_head != nullptr) {
      auto next = m_head->next;
      m_delete(m_head);
      m_head = next;
    }
  }

  template <class F>
  void clear_cached_elements(const F& f) {
    while (m_head) {
      auto next = m_head->next;
      f(*m_head);
      m_delete(m_head);
      m_head = next;
    }
  }

  pointer stack_empty_dummy() {
    // we are *never* going to dereference the returned pointer;
    // it is only used as indicator wheter this queue is closed or not
    return reinterpret_cast<pointer>(this);
  }

  pointer reader_blocked_dummy() {
    // we are not going to dereference this pointer either
    return reinterpret_cast<pointer>(reinterpret_cast<intptr_t>(this)
                                     + sizeof(void*));
  }

  bool is_dummy(pointer ptr) {
    return ptr == stack_empty_dummy() || ptr == reader_blocked_dummy();
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_SINGLE_READER_QUEUE_HPP
