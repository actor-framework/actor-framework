/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "caf/config.hpp"

#include "caf/intrusive/inbox_result.hpp"

namespace caf {
namespace intrusive {

/// An intrusive, thread-safe LIFO queue implementation for a single reader
/// with any number of writers.
template <class Policy>
class lifo_inbox {
public:
  using policy_type = Policy;

  using value_type = typename policy_type::mapped_type;

  using pointer = value_type*;

  using node_type = typename value_type::node_type;

  using node_pointer = node_type*;

  using unique_pointer = typename policy_type::unique_pointer;

  using deleter_type = typename unique_pointer::deleter_type;

  /// Tries to enqueue a new element to the inbox.
  /// @threadsafe
  inbox_result push_front(pointer new_element) noexcept {
    CAF_ASSERT(new_element != nullptr);
    pointer e = stack_.load();
    auto eof = stack_closed_tag();
    auto blk = reader_blocked_tag();
    while (e != eof) {
      // A tag is never part of a non-empty list.
      new_element->next = e != blk ? e : nullptr;
      if (stack_.compare_exchange_strong(e, new_element))
        return  e == reader_blocked_tag() ? inbox_result::unblocked_reader
                                            : inbox_result::success;
      // Continue with new value of `e`.
    }
    // The queue has been closed, drop messages.
    deleter_type d;
    d(new_element);
    return inbox_result::queue_closed;
  }

  /// Tries to enqueue a new element to the inbox.
  /// @threadsafe
  inbox_result push_front(unique_pointer x) noexcept {
    return push_front(x.release());
  }


  /// Tries to enqueue a new element to the mailbox.
  /// @threadsafe
  template <class... Ts>
  inbox_result emplace_front(Ts&&... xs) {
    return push_front(new value_type(std::forward<Ts>(xs)...));
  }

  /// Queries whether this queue is empty.
  /// @pre `!closed() && !blocked()`
  bool empty() const noexcept {
    CAF_ASSERT(!closed());
    CAF_ASSERT(!blocked());
    return stack_.load() == stack_empty_tag();
  }

  /// Queries whether this has been closed.
  bool closed() const noexcept {
    return stack_.load() == stack_closed_tag();
  }

  /// Queries whether this has been marked as blocked, i.e.,
  /// the owner of the list is waiting for new data.
  bool blocked() const noexcept {
    return stack_.load() == reader_blocked_tag();
  }

  /// Tries to set this queue from `empty` to `blocked`.
  bool try_block() noexcept {
    auto e = stack_empty_tag();
    return stack_.compare_exchange_strong(e, reader_blocked_tag());
  }

  /// Tries to set this queue from `blocked` to `empty`.
  bool try_unblock() noexcept {
    auto e = reader_blocked_tag();
    return stack_.compare_exchange_strong(e, stack_empty_tag());
  }

  /// Sets the head to `new_head` and returns the previous head if the queue
  /// was not empty.
  pointer take_head(pointer new_head) noexcept {
    // This member function should only be used to transition to closed or
    // empty.
    CAF_ASSERT(new_head == stack_closed_tag()
               || new_head == stack_empty_tag());
    pointer e = stack_.load();
    // Must not be called on a closed queue.
    CAF_ASSERT(e != stack_closed_tag());
    // Must not be called on a blocked queue unless for setting it to closed,
    // because that would mean an actor accesses its mailbox after blocking its
    // mailbox but before receiving anything.
    CAF_ASSERT(e != reader_blocked_tag() || new_head == stack_closed_tag());
    // We don't assert these conditions again since only the owner is allowed
    // to call this member function, i.e., there's never a race on `take_head`.
    while (e != new_head) {
      if (stack_.compare_exchange_weak(e, new_head)) {
        CAF_ASSERT(e != stack_closed_tag());
        if (is_empty_or_blocked_tag(e)) {
          // Sanity check: going from empty/blocked to closed.
          CAF_ASSERT(new_head == stack_closed_tag());
          return nullptr;
        }
        return e;
      }
      // Next iteration.
    }
    return nullptr;
  }

  /// Sets the head to `stack_empty_tag()` and returns the previous head if
  /// the queue was not empty.
  pointer take_head() noexcept {
    return take_head(stack_empty_tag());
  }

  /// Closes this queue and deletes all remaining elements.
  /// @warning Call only from the reader (owner).
  void close() noexcept {
    deleter_type d;
    // We assume the node destructor to never throw. However, the following
    // static assert fails. Unfortunately, std::default_delete's apply operator
    // is not noexcept (event for types that have a noexcept destructor).
    // static_assert(noexcept(d(std::declval<pointer>())),
    //               "deleter is not noexcept");
    close(d);
  }

  /// Closes this queue and applies `f` to each pointer. The function object
  /// `f` must take ownership of the passed pointer.
  /// @warning Call only from the reader (owner).
  template <class F>
  void close(F& f) noexcept(noexcept(f(std::declval<pointer>()))) {
    node_pointer ptr = take_head(stack_closed_tag());
    while (ptr != nullptr) {
      auto next = ptr->next;
      f(promote(ptr));
      ptr = next;
    }
  }

  lifo_inbox() noexcept {
    stack_ = stack_empty_tag();
  }

  ~lifo_inbox() noexcept {
    if (!closed())
      close();
  }

  // -- synchronized access ---------------------------------------------------

  template <class Mutex, class CondVar>
  bool synchronized_push_front(Mutex& mtx, CondVar& cv, pointer ptr) {
    switch (push_front(ptr)) {
      default:
        // enqueued message to a running actor's mailbox
        return true;
      case inbox_result::unblocked_reader: {
        std::unique_lock<Mutex> guard(mtx);
        cv.notify_one();
        return true;
      }
      case inbox_result::queue_closed:
        // actor no longer alive
        return false;
    }
  }

  template <class Mutex, class CondVar>
  bool synchronized_push_front(Mutex& mtx, CondVar& cv, unique_pointer ptr) {
    return synchronized_push_front(mtx, cv, ptr.relase());
  }

  template <class Mutex, class CondVar, class... Ts>
  bool synchronized_emplace_front(Mutex& mtx, CondVar& cv, Ts&&... xs) {
    return synchronized_push_front(mtx, cv,
                                   new value_type(std::forward<Ts>(xs)...));
  }

  template <class Mutex, class CondVar>
  void synchronized_await(Mutex& mtx, CondVar& cv) {
    CAF_ASSERT(!closed());
    if (try_block()) {
      std::unique_lock<Mutex> guard(mtx);
      while (blocked())
        cv.wait(guard);
    }
  }

  template <class Mutex, class CondVar, class TimePoint>
  bool synchronized_await(Mutex& mtx, CondVar& cv, const TimePoint& timeout) {
    CAF_ASSERT(!closed());
    if (try_block()) {
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
  static constexpr pointer stack_empty_tag() {
    // We are *never* going to dereference the returned pointer. It is only
    // used as indicator wheter this queue is empty or not.
    return static_cast<pointer>(nullptr);
  }

  pointer stack_closed_tag() const noexcept {
    // We are *never* going to dereference the returned pointer. It is only
    // used as indicator wheter this queue is closed or not.
    return reinterpret_cast<pointer>(reinterpret_cast<intptr_t>(this) + 1);
  }

  pointer reader_blocked_tag() const noexcept {
    // We are *never* going to dereference the returned pointer. It is only
    // used as indicator wheter the owner of the queue is currently waiting for
    // new messages.
    return reinterpret_cast<pointer>(const_cast<lifo_inbox*>(this));
  }

  bool is_empty_or_blocked_tag(pointer x) const noexcept {
    return x == stack_empty_tag() || x == reader_blocked_tag();
  }

  // -- member variables ------------------------------------------------------

  std::atomic<pointer> stack_;
};

} // namespace intrusive
} // namespace caf

