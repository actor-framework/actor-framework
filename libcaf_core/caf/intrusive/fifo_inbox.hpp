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

#include "caf/config.hpp"

#include <list>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <limits>
#include <condition_variable> // std::cv_status

#include "caf/config.hpp"

#include "caf/intrusive/inbox_result.hpp"
#include "caf/intrusive/lifo_inbox.hpp"
#include "caf/intrusive/new_round_result.hpp"

#include "caf/detail/enqueue_result.hpp"

namespace caf {
namespace intrusive {

/// A FIFO inbox that combines an efficient thread-safe LIFO inbox with a FIFO
/// queue for re-ordering incoming messages.
template <class Policy>
class fifo_inbox {
public:
  // -- member types -----------------------------------------------------------

  using policy_type = Policy;

  using queue_type = typename policy_type::queue_type;

  using deficit_type = typename policy_type::deficit_type;

  using value_type = typename policy_type::mapped_type;

  using lifo_inbox_type = lifo_inbox<policy_type>;

  using pointer = value_type*;

  using unique_pointer = typename queue_type::unique_pointer;

  using node_pointer = typename value_type::node_pointer;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  fifo_inbox(Ts&&... xs) : queue_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- queue and stack status functions ---------------------------------------

  /// Returns an approximation of the current size.
  size_t size() noexcept {
    fetch_more();
    return queue_.total_task_size();
  }

  /// Queries whether the inbox is empty.
  bool empty() const noexcept {
    return queue_.empty() && inbox_.empty();
  }

  /// Queries whether this inbox is closed.
  bool closed() const noexcept {
    return inbox_.closed();
  }

  /// Queries whether this has been marked as blocked, i.e.,
  /// the owner of the list is waiting for new data.
  bool blocked() const noexcept {
    return inbox_.blocked();
  }

  /// Appends `ptr` to the inbox.
  inbox_result push_back(pointer ptr) noexcept {
    return inbox_.push_front(ptr);
  }

  /// Appends `ptr` to the inbox.
  inbox_result push_back(unique_pointer ptr) noexcept {
    return push_back(ptr.release());
  }

  template <class... Ts>
  inbox_result emplace_back(Ts&&... xs) {
    return push_back(new value_type(std::forward<Ts>(xs)...));
  }

  // -- backwards compatibility ------------------------------------------------

  /// @cond PRIVATE

  detail::enqueue_result enqueue(pointer ptr) noexcept {
    return static_cast<detail::enqueue_result>(inbox_.push_front(ptr));
  }

  size_t count() noexcept {
    return size();
  }

  size_t count(size_t) noexcept {
    return size();
  }

  /// @endcond

  // -- queue management -------------------------------------------------------

  void flush_cache() noexcept {
    queue_.flush_cache();
  }

  /// Tries to get more items from the inbox.
  bool fetch_more() {
    node_pointer head = inbox_.take_head();
    if (head == nullptr)
      return false;
    do {
      auto next = head->next;
      queue_.lifo_append(promote(head));
      head = next;
    } while (head != nullptr);
    queue_.stop_lifo_append();
    return true;
  }

  /// Tries to set this queue from `empty` to `blocked`.
  bool try_block() {
    return queue_.empty() && inbox_.try_block();
  }

  /// Tries to set this queue from `blocked` to `empty`.
  bool try_unblock() {
    return inbox_.try_unblock();
  }

  /// Closes this inbox and moves all elements to the queue.
  /// @warning Call only from the reader (owner).
  void close() {
    auto f = [&](pointer x) {
      queue_.lifo_append(x);
    };
    inbox_.close(f);
    queue_.stop_lifo_append();
  }

  /// Run a new round with `quantum`, dispatching all tasks to `consumer`.
  template <class F>
  new_round_result new_round(deficit_type quantum, F& consumer) {
    fetch_more();
    return queue_.new_round(quantum, consumer);
  }

  pointer peek() noexcept {
    fetch_more();
    return queue_.peek();
  }

  queue_type& queue() noexcept {
    return queue_;
  }

  // -- synchronized access ----------------------------------------------------

  template <class Mutex, class CondVar>
  bool synchronized_push_back(Mutex& mtx, CondVar& cv, pointer ptr) {
    return inbox_.synchronized_push_front(mtx, cv, ptr);
  }

  template <class Mutex, class CondVar>
  bool synchronized_push_back(Mutex& mtx, CondVar& cv, unique_pointer ptr) {
    return synchronized_push_back(mtx, cv, ptr.release());
  }

  template <class Mutex, class CondVar, class... Ts>
  bool synchronized_emplace_back(Mutex& mtx, CondVar& cv, Ts&&... xs) {
    return synchronized_push_back(mtx, cv,
                                  new value_type(std::forward<Ts>(xs)...));
  }

  template <class Mutex, class CondVar>
  void synchronized_await(Mutex& mtx, CondVar& cv) {
    if (queue_.empty()) {
      inbox_.synchronized_await(mtx, cv);
      fetch_more();
    }
  }

  template <class Mutex, class CondVar, class TimePoint>
  bool synchronized_await(Mutex& mtx, CondVar& cv, const TimePoint& timeout) {
    if (!queue_.empty())
      return true;
    if (inbox_.synchronized_await(mtx, cv, timeout)) {
      fetch_more();
      return true;
    }
    return false;
  }

private:
  // -- member variables -------------------------------------------------------

  /// Thread-safe LIFO inbox.
  lifo_inbox_type inbox_;

  /// User-facing queue that is constantly resupplied from the inbox.
  queue_type queue_;
};

} // namespace intrusive
} // namespace caf

