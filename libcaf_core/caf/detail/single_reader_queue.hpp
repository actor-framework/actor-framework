/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <limits>
#include <condition_variable> // std::cv_status

#include "caf/config.hpp"

#include "caf/detail/intrusive_partitioned_list.hpp"

namespace caf {
namespace detail {

/// Denotes in which state queue and reader are after an enqueue.
enum class enqueue_result {
  /// Indicates that the enqueue operation succeeded and
  /// the reader is ready to receive the data.
  success,

  /// Indicates that the enqueue operation succeeded and
  /// the reader is currently blocked, i.e., needs to be re-scheduled.
  unblocked_reader,

  /// Indicates that the enqueue operation failed because the
  /// queue has been closed by the reader.
  queue_closed
};

/// An intrusive, thread-safe queue implementation.
template <class T, class Delete = std::default_delete<T>>
class single_reader_queue {
public:
  using value_type = T;
  using pointer = value_type*;
  using deleter_type = Delete;
  using unique_pointer = std::unique_ptr<value_type, deleter_type>;
  using cache_type = intrusive_partitioned_list<value_type, deleter_type>;

  /// Tries to dequeue a new element from the mailbox.
  /// @warning Call only from the reader (owner).
  pointer try_pop() {
    return take_head();
  }

  /// Tries to enqueue a new element to the mailbox.
  /// @warning Call only from the reader (owner).
  enqueue_result enqueue(pointer new_element) {
    CAF_ASSERT(new_element != nullptr);
    pointer e = stack_.load();
    for (;;) {
      if (! e) {
        // if tail is nullptr, the queue has been closed
        delete_(new_element);
        return enqueue_result::queue_closed;
      }
      // a dummy is never part of a non-empty list
      new_element->next = is_dummy(e) ? nullptr : e;
      if (stack_.compare_exchange_strong(e, new_element)) {
        return  (e == reader_blocked_dummy()) ? enqueue_result::unblocked_reader
                                              : enqueue_result::success;
      }
      // continue with new value of e
    }
  }

  /// Queries whether there is new data to read, i.e., whether the next
  /// call to {@link try_pop} would succeeed.
  /// @pre !closed()
  bool can_fetch_more() {
    if (head_ != nullptr) {
      return true;
    }
    auto ptr = stack_.load();
    CAF_ASSERT(ptr != nullptr);
    return ! is_dummy(ptr);
  }

  /// Queries whether this queue is empty.
  /// @warning Call only from the reader (owner).
  bool empty() {
    CAF_ASSERT(! closed());
    return cache_.empty() && head_ == nullptr && is_dummy(stack_.load());
  }

  /// Queries whether this has been closed.
  bool closed() {
    return stack_.load() == nullptr;
  }

  /// Queries whether this has been marked as blocked, i.e.,
  /// the owner of the list is waiting for new data.
  bool blocked() {
    return stack_.load() == reader_blocked_dummy();
  }

  /// Tries to set this queue from state `empty` to state `blocked`.
  bool try_block() {
    auto e = stack_empty_dummy();
    bool res = stack_.compare_exchange_strong(e, reader_blocked_dummy());
    CAF_ASSERT(e != nullptr);
    // return true in case queue was already blocked
    return res || e == reader_blocked_dummy();
  }

  /// Tries to set this queue from state `blocked` to state `empty`.
  bool try_unblock() {
    auto e = reader_blocked_dummy();
    return stack_.compare_exchange_strong(e, stack_empty_dummy());
  }

  /// Closes this queue and deletes all remaining elements.
  /// @warning Call only from the reader (owner).
  void close() {
    auto nop = [](const T&) { };
    close(nop);
  }

  /// Closes this queue and applies f to all remaining
  ///        elements before deleting them.
  /// @warning Call only from the reader (owner).
  template <class F>
  void close(const F& f) {
    clear_cached_elements(f);
    if (fetch_new_data(nullptr)) {
      clear_cached_elements(f);
    }
    cache_.clear(f);
  }

  single_reader_queue() : head_(nullptr) {
    stack_ = stack_empty_dummy();
  }

  ~single_reader_queue() {
    if (! closed()) {
      close();
    }
  }

  size_t count(size_t max_count = std::numeric_limits<size_t>::max()) {
    size_t res = cache_.count(max_count);
    if (res >= max_count) {
      return res;
    }
    fetch_new_data();
    auto ptr = head_;
    while (ptr && res < max_count) {
      ptr = ptr->next;
      ++res;
    }
    return res;
  }

  // note: the cache is intended to be used by the owner, the queue itself
  //       never accesses the cache other than for counting;
  //       the first partition of the cache is meant to be used to store and
  //       sort messages that were not processed yet, while the second
  //       partition is meant to store skipped messages
  cache_type& cache() {
    return cache_;
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
    CAF_ASSERT(! closed());
    if (! can_fetch_more() && try_block()) {
      std::unique_lock<Mutex> guard(mtx);
      while (blocked()) {
        cv.wait(guard);
      }
    }
  }

  template <class Mutex, class CondVar, class TimePoint>
  bool synchronized_await(Mutex& mtx, CondVar& cv, const TimePoint& timeout) {
    CAF_ASSERT(! closed());
    if (! can_fetch_more() && try_block()) {
      std::unique_lock<Mutex> guard(mtx);
      while (blocked()) {
        if (cv.wait_until(guard, timeout) == std::cv_status::timeout) {
          // if we're unable to set the queue from blocked to empty,
          // than there's a new element in the list
          return ! try_unblock();
        }
      }
    }
    return true;
  }

private:
  // exposed to "outside" access
  std::atomic<pointer> stack_;

  // accessed only by the owner
  pointer head_;
  deleter_type delete_;
  intrusive_partitioned_list<value_type, deleter_type> cache_;

  // atomically sets stack_ back and enqueues all elements to the cache
  bool fetch_new_data(pointer end_ptr) {
    CAF_ASSERT(end_ptr == nullptr || end_ptr == stack_empty_dummy());
    pointer e = stack_.load();
    // must not be called on a closed queue
    CAF_ASSERT(e != nullptr);
    // fetching data while blocked is an error
    CAF_ASSERT(e != reader_blocked_dummy());
    // it's enough to check this once, since only the owner is allowed
    // to close the queue and only the owner is allowed to call this
    // member function
    while (e != end_ptr) {
      if (stack_.compare_exchange_weak(e, end_ptr)) {
        // fetching data while blocked is an error
        CAF_ASSERT(e != reader_blocked_dummy());
        if (is_dummy(e)) {
          // only use-case for this is closing a queue
          CAF_ASSERT(end_ptr == nullptr);
          return false;
        }
        while (e) {
          CAF_ASSERT(! is_dummy(e));
          auto next = e->next;
          e->next = head_;
          head_ = e;
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
    if (head_ != nullptr || fetch_new_data()) {
      auto result = head_;
      head_ = head_->next;
      return result;
    }
    return nullptr;
  }

  template <class F>
  void clear_cached_elements(const F& f) {
    while (head_) {
      auto next = head_->next;
      f(*head_);
      delete_(head_);
      head_ = next;
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
                                     + static_cast<intptr_t>(sizeof(void*)));
  }

  bool is_dummy(pointer ptr) {
    return ptr == stack_empty_dummy() || ptr == reader_blocked_dummy();
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_SINGLE_READER_QUEUE_HPP
