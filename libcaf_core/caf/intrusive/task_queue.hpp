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

#include <utility>

#include "caf/config.hpp"

#include "caf/intrusive/forward_iterator.hpp"

namespace caf {
namespace intrusive {

/// A singly-linked FIFO queue for storing tasks of varying size. This queue is
/// used as a base type for concrete task abstractions such as `drr_queue` and
/// therefore has no dequeue functions.
template <class Policy>
class task_queue {
public:
  // -- member types ----------------------------------------------------------

  using policy_type = Policy;

  using value_type = typename policy_type::mapped_type;

  using node_type = typename value_type::node_type;

  using node_pointer = node_type*;

  using pointer = value_type*;

  using const_pointer = const value_type*;

  using reference = value_type&;

  using const_reference = const value_type&;

  using unique_pointer = typename policy_type::unique_pointer;

  using task_size_type = typename policy_type::task_size_type;

  using iterator = forward_iterator<value_type>;

  using const_iterator = forward_iterator<const value_type>;

  // -- constructors, destructors, and assignment operators -------------------

  task_queue(policy_type p)
      : old_last_(nullptr),
        new_head_(nullptr),
        policy_(std::move(p)) {
    init();
  }

  task_queue(task_queue&& other) : task_queue(other.policy()) {
    if (other.empty()) {
      init();
    } else {
      head_.next = other.head_.next;
      tail_.next = other.tail_.next;
      tail_.next->next = &tail_;
      total_task_size_ = other.total_task_size_;
      other.init();
    }
  }

  task_queue& operator=(task_queue&& other) {
    deinit();
    if (other.empty()) {
      init();
    } else {
      head_.next = other.head_.next;
      tail_.next = other.tail_.next;
      tail_.next->next = &tail_;
      total_task_size_ = other.total_task_size_;
      other.init();
    }
    return *this;
  }

  virtual ~task_queue() {
    deinit();
  }

  // -- observers -------------------------------------------------------------

  /// Returns the policy object.
  policy_type& policy() noexcept {
    return policy_;
  }

  /// Returns the policy object.
  const policy_type& policy() const noexcept {
    return policy_;
  }

  /// Returns the accumulated size of all stored tasks.
  task_size_type total_task_size() const noexcept {
    return total_task_size_;
  }

  /// Returns whether the queue has no elements.
  bool empty() const noexcept {
    return total_task_size() == 0;
  }

  /// Peeks at the first element in the queue. Returns `nullptr` if the queue is
  /// empty.
  pointer peek() noexcept {
    auto ptr = head_.next;
    return ptr != &tail_ ? promote(ptr) : nullptr;
  }

  // -- modifiers -------------------------------------------------------------

  /// Removes all elements from the queue.
  void clear() {
    deinit();
    init();
  }

  /// @private
  void inc_total_task_size(task_size_type x) noexcept {
    CAF_ASSERT(x > 0);
    total_task_size_ += x;
  }

  /// @private
  void inc_total_task_size(const value_type& x) noexcept {
    inc_total_task_size(policy_.task_size(x));
  }

  /// @private
  void dec_total_task_size(task_size_type x) noexcept {
    CAF_ASSERT(x > 0);
    total_task_size_ -= x;
  }

  /// @private
  void dec_total_task_size(const value_type& x) noexcept {
    dec_total_task_size(policy_.task_size(x));
  }

  /// @private
  unique_pointer next(task_size_type& deficit) noexcept {
    unique_pointer result;
    if (!empty()) {
      auto ptr = promote(head_.next);
      auto ts = policy_.task_size(*ptr);
      CAF_ASSERT(ts > 0);
      if (ts <= deficit) {
        deficit -= ts;
        total_task_size_ -= ts;
        head_.next = ptr->next;
        if (total_task_size_ == 0) {
          CAF_ASSERT(head_.next == &(tail_));
          deficit = 0;
          tail_.next = &(head_);
        }
        result.reset(ptr);
      }
    }
    return result;
  }

  // -- iterator access --------------------------------------------------------

  /// Returns an iterator to the dummy before the first element.
  iterator before_begin() noexcept {
    return &head_;
  }

  /// Returns an iterator to the dummy before the first element.
  const_iterator before_begin() const noexcept {
    return &head_;
  }

  /// Returns an iterator to the dummy before the first element.
  iterator begin() noexcept {
    return head_.next;
  }

  /// Returns an iterator to the dummy before the first element.
  const_iterator begin() const noexcept {
    return head_.next;
  }

  /// Returns an iterator to the dummy before the first element.
  const_iterator cbegin() const noexcept {
    return head_.next;
  }

  /// Returns a pointer to the dummy past the last element.
  iterator end() noexcept {
    return &tail_;
  }

  /// Returns a pointer to the dummy past the last element.
  const_iterator end() const noexcept {
    return &tail_;
  }

  /// Returns a pointer to the dummy past the last element.
  const_iterator cend() const noexcept {
    return &tail_;
  }

  // -- element access ---------------------------------------------------------

  /// Returns a pointer to the first element.
  pointer front() noexcept {
    return promote(head_.next);
  }

  /// Returns a pointer to the last element.
  pointer back() noexcept {
    return promote(tail_.next);
  }

  // -- insertion --------------------------------------------------------------

  /// Appends `ptr` to the queue.
  /// @pre `ptr != nullptr`
  bool push_back(pointer ptr) noexcept {
    CAF_ASSERT(ptr != nullptr);
    tail_.next->next = ptr;
    tail_.next = ptr;
    ptr->next = &tail_;
    inc_total_task_size(*ptr);
    return true;
  }

  /// Appends `ptr` to the queue.
  /// @pre `ptr != nullptr`
  bool push_back(unique_pointer ptr) noexcept {
    return push_back(ptr.release());
  }

  /// Creates a new element from `xs...` and appends it.
  template <class... Ts>
  bool emplace_back(Ts&&... xs) {
    return push_back(new value_type(std::forward<Ts>(xs)...));
  }

  /// Transfers all element from `other` to the front of this queue.
  template <class Container>
  void prepend(Container& other) {
    if (other.empty())
      return;
    if (empty()) {
      *this = std::move(other);
      return;
    }
    other.back()->next = head_.next;
    head_.next = other.front();
    inc_total_task_size(other.total_task_size());
    other.init();
  }

  /// Transfers all element from `other` to the back of this queue.
  template <class Container>
  void append(Container& other) {
    if (other.empty())
      return;
    if (empty()) {
      *this = std::move(other);
      return;
    }
    back()->next = other.front();
    other.back()->next = &tail_;
    tail_.next = other.back();
    inc_total_task_size(other.total_task_size());
    other.init();
  }

  /// Allows to insert a LIFO-ordered sequence at the end of the queue by
  /// calling this member function multiple times. Converts the order to FIFO
  /// on the fly.
  /// @warning leaves the queue in an invalid state until calling
  ///          `stop_lifo_append`.
  /// @private
  void lifo_append(node_pointer ptr) {
    if (old_last_ == nullptr) {
      old_last_ = back();
      push_back(promote(ptr));
    } else {
      ptr->next = new_head_;
      inc_total_task_size(*promote(ptr));
    }
    new_head_ = ptr;
  }

  /// Restores a consistent state of the queue after calling `lifo_append`.
  /// @private
  void stop_lifo_append() {
    if (old_last_ != nullptr) {
      CAF_ASSERT(new_head_ != nullptr);
      old_last_->next = new_head_;
      old_last_ = nullptr;
    }
  }

  // -- construction and destruction helper -----------------------------------

  /// Restores a consistent, empty state.
  /// @private
  void init() noexcept {
    head_.next = &tail_;
    tail_.next = &head_;
    total_task_size_ = 0;
  }

  /// Deletes all elements.
  /// @warning leaves the queue in an invalid state until calling `init` again.
  /// @private
  void deinit() noexcept {
    for (auto i = head_.next; i != &tail_;) {
      auto ptr = i;
      i = i->next;
      typename unique_pointer::deleter_type d;
      d(promote(ptr));
    }
  }

protected:
  // -- member variables ------------------------------------------------------

  /// node element pointing to the first element.
  node_type head_;

  /// node element pointing past the last element.
  node_type tail_;

  /// Stores the total size of all items in the queue.
  task_size_type total_task_size_;

  /// Used for LIFO -> FIFO insertion.
  node_pointer old_last_;

  /// Used for LIFO -> FIFO insertion.
  node_pointer new_head_;

  /// Manipulates instances of T.
  policy_type policy_;
};

} // namespace intrusive
} // namespace caf

