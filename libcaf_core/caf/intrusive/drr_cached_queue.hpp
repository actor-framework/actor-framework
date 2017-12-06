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

#ifndef CAF_INTRUSIVE_DRR_CACHED_QUEUE_HPP
#define CAF_INTRUSIVE_DRR_CACHED_QUEUE_HPP

#include <utility>

#include "caf/config.hpp"

#include "caf/intrusive/task_queue.hpp"
#include "caf/intrusive/task_result.hpp"

namespace caf {
namespace intrusive {

/// A Deficit Round Robin queue with an internal cache for allowing skipping
/// consumers.
template <class Policy>
class drr_cached_queue { // Note that we do *not* inherit from
                         // task_queue<Policy>, because the cached queue can no
                         // longer offer iterator access.
public:
  // -- member types ----------------------------------------------------------
  using policy_type = Policy;

  using deleter_type = typename policy_type::deleter_type;

  using value_type = typename policy_type::mapped_type;

  using node_type = typename value_type::node_type;

  using node_pointer = node_type*;

  using pointer = value_type*;

  using unique_pointer = typename policy_type::unique_pointer;

  using deficit_type = typename policy_type::deficit_type;

  using task_size_type = typename policy_type::task_size_type;

  using list_type = task_queue<policy_type>;

  using cache_type = task_queue<policy_type>;

  // -- constructors, destructors, and assignment operators -------------------

  drr_cached_queue(policy_type p)
      : list_(p),
        deficit_(0),
        cache_(std::move(p)) {
    // nop
  }

  drr_cached_queue(drr_cached_queue&& other)
      : list_(std::move(other.list_)),
        deficit_(other.deficit_),
        cache_(std::move(other.cache_)) {
    // nop
  }

  drr_cached_queue& operator=(drr_cached_queue&& other) {
    list_ = std::move(other.list_);
    deficit_ = other.deficit_;
    cache_ = std::move(other.cache_);
    return *this;
  }

  // -- observers -------------------------------------------------------------

  /// Returns the policy object.
  policy_type& policy() noexcept {
    return list_.policy();
  }

  /// Returns the policy object.
  const policy_type& policy() const noexcept {
    return list_.policy();
  }

  deficit_type deficit() const {
    return deficit_;
  }

  /// Returns the accumulated size of all stored tasks in the list, i.e., tasks
  /// that are not in the cache.
  task_size_type total_task_size() const {
    return list_.total_task_size();
  }

  /// Returns whether the queue has no uncached tasks.
  bool empty() const noexcept {
    return total_task_size() == 0;
  }

  /// Peeks at the first element of the cache or of the list in case the former
  /// is empty.
  pointer peek() noexcept {
    auto ptr = cache_.peek();
    return ptr == nullptr ? list_.peek() : ptr;
  }

  /// Applies `f` to each element in the queue, including cached elements.
  template <class F>
  void peek_all(F f) const {
    for (auto i = cache_.begin(); i != cache_.end(); ++i)
      f(*promote(i.ptr));
    for (auto i = list_.begin(); i != list_.end(); ++i)
      f(*promote(i.ptr));
  }

  // -- modifiers -------------------------------------------------------------

  /// Removes all elements from the queue.
  void clear() {
    list_.clear();
    cache_.clear();
  }

  void inc_deficit(deficit_type x) noexcept {
    if (!list_.empty())
      deficit_ += x;
  }

  void flush_cache() noexcept {
    list_.prepend(cache_);
  }

  /// @private
  template <class T>
  void inc_total_task_size(T&& x) noexcept {
    list_.inc_total_task_size(std::forward<T>(x));
  }

  /// @private
  template <class T>
  void dec_total_task_size(T&& x) noexcept {
    list_.dec_total_task_size(std::forward<T>(x));
  }

  /// Takes the first element out of the queue if the deficit allows it and
  /// returns the element.
  unique_pointer take_front() noexcept {
    list_.prepend(cache_);
    unique_pointer result;
    if (!list_.empty()) {
      auto ptr = list_.front();
      auto ts = policy().task_size(*ptr);
      CAF_ASSERT(ts > 0);
      if (ts <= deficit_) {
        deficit_ -= ts;
        dec_total_task_size(ts);
        list_.before_begin()->next = ptr->next;
        if (total_task_size() == 0) {
          CAF_ASSERT(list_.begin() == list_.end());
          deficit_ = 0;
          list_.end()->next = list_.before_begin().ptr;
        }
        result.reset(ptr);
      }
    }
    return result;
  }

  /// Consumes items from the queue until the queue is empty, there is not
  /// enough deficit to dequeue the next task or the consumer returns `stop`.
  /// @returns `true` if `f` consumed at least one item.
  template <class F>
  bool consume(F& f) noexcept(noexcept(f(std::declval<value_type&>()))) {
    long consumed = 0;
    if (!cache_.empty())
      list_.prepend(cache_);
    if (!list_.empty()) {
      auto ptr = list_.front();
      auto ts = policy().task_size(*ptr);
      while (ts <= deficit_) {
        CAF_ASSERT(ts > 0);
        auto next = ptr->next;
        dec_total_task_size(ts);
        // Make sure the queue is in a consistent state before calling `f` in
        // case `f` recursively calls consume.
        list_.before_begin()->next = next;
        if (total_task_size() == 0) {
          CAF_ASSERT(list_.begin() == list_.end());
          list_.end()->next = list_.begin().ptr;
        }
        // Always decrease the deficit_ counter, again because `f` is allowed
        // to call consume again.
        deficit_ -= ts;
        auto res = f(*ptr);
        if (res == task_result::skip) {
          // Fix deficit and push the unconsumed item to the cache.
          deficit_ += ts;
          cache_.push_back(ptr);
          if (list_.empty()) {
            deficit_ = 0;
            return consumed != 0;
          }
        } else {
          deleter_type d;
          d(ptr);
          ++consumed;
          if (!cache_.empty())
            list_.prepend(cache_);
          if (list_.empty()) {
            deficit_ = 0;
            return consumed != 0;
          }
          if (res == task_result::stop)
            return consumed != 0;
        }
        // Next iteration.
        ptr = list_.front();
        ts = policy().task_size(*ptr);
      }
    }
    return consumed != 0;
  }

  /// Run a new round with `quantum`, dispatching all tasks to `consumer`.
  template <class F>
  bool new_round(deficit_type quantum, F& consumer)
  noexcept(noexcept(consumer(std::declval<value_type&>()))) {
    if (!list_.empty()) {
      deficit_ += quantum;
      return consume(consumer);
    }
    return false;
  }

  cache_type& cache() noexcept {
    return cache_;
  }

  // -- insertion --------------------------------------------------------------

  /// Appends `ptr` to the queue.
  /// @pre `ptr != nullptr`
  bool push_back(pointer ptr) noexcept {
    return list_.push_back(ptr);
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

  /// @private
  void lifo_append(node_pointer ptr) {
    list_.lifo_append(ptr);
  }

  /// @private
  void stop_lifo_append() {
    list_.stop_lifo_append();
  }

private:
  // -- member variables ------------------------------------------------------
  /// Stores current (unskipped) items.
  list_type list_;

  /// Stores the deficit on this queue.
  deficit_type deficit_ = 0;

  /// Stores previously skipped items.
  cache_type cache_;
};

} // namespace intrusive
} // namespace caf

#endif // CAF_INTRUSIVE_DRR_CACHED_QUEUE_HPP
