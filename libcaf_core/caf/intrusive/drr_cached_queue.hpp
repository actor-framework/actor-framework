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

#include "caf/intrusive/task_queue.hpp"
#include "caf/intrusive/task_result.hpp"

namespace caf {
namespace intrusive {

/// A Deficit Round Robin queue with an internal cache for allowing skipping
/// consumers.
template <class Policy>
class drr_cached_queue : public task_queue<Policy> {
public:
  // -- member types ----------------------------------------------------------

  using super = task_queue<Policy>;

  using typename super::policy_type;

  using typename super::deleter_type;

  using typename super::unique_pointer;

  using typename super::value_type;

  using cache_type = task_queue<Policy>;

  using deficit_type = typename policy_type::deficit_type;

  using task_size_type = typename policy_type::task_size_type;

  // -- constructors, destructors, and assignment operators -------------------

  drr_cached_queue(const policy_type& p) : super(p), deficit_(0), cache_(p) {
    // nop
  }

  drr_cached_queue(drr_cached_queue&& other)
      : super(std::move(other)),
        deficit_(0) {
    // nop
  }

  drr_cached_queue& operator=(drr_cached_queue&& other) {
    super::operator=(std::move(other));
    return *this;
  }

  // -- observers -------------------------------------------------------------

  deficit_type deficit() const {
    return deficit_;
  }

  // -- modifiers -------------------------------------------------------------

  void inc_deficit(deficit_type x) noexcept {
    if (!super::empty())
      deficit_ += x;
  }

  void flush_cache() noexcept {
    super::prepend(cache_);
  }

  /// Takes the first element out of the queue if the deficit allows it and
  /// returns the element.
  unique_pointer take_front() noexcept {
    super::prepend(cache_);
    unique_pointer result;
    if (!super::empty()) {
      auto ptr = promote(super::head_.next);
      auto ts = super::policy_.task_size(*ptr);
      CAF_ASSERT(ts > 0);
      if (ts <= deficit_) {
        deficit_ -= ts;
        super::total_task_size_ -= ts;
        super::head_.next = ptr->next;
        if (super::total_task_size_ == 0) {
          CAF_ASSERT(super::head_.next == &(super::tail_));
          deficit_ = 0;
          super::tail_.next = &(super::head_);
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
      super::prepend(cache_);
    if (!super::empty()) {
      auto ptr = promote(super::head_.next);
      auto ts = super::policy_.task_size(*ptr);
      while (ts <= deficit_) {
        auto next = ptr->next;
        super::total_task_size_ -= ts;
        // Make sure the queue is in a consistent state before calling `f` in
        // case `f` recursively calls consume.
        super::head_.next = next;
        if (super::total_task_size_ == 0) {
          CAF_ASSERT(super::head_.next == &(super::tail_));
          super::tail_.next = &(super::head_);
        }
        // Always decrease the deficit_ counter, again because `f` is allowed
        // to call consume again.
        deficit_ -= ts;
        auto res = f(*ptr);
        if (res == task_result::skip) {
          // Fix deficit and push the unconsumed item to the cache.
          deficit_ += ts;
          cache_.push_back(ptr);
          if (super::empty()) {
            deficit_ = 0;
            return consumed != 0;
          }
        } else {
          deleter_type d;
          d(ptr);
          ++consumed;
          if (!cache_.empty())
            super::prepend(cache_);
          if (super::empty()) {
            deficit_ = 0;
            return consumed != 0;
          }
          if (res == task_result::stop)
            return consumed != 0;
        }
        // Next iteration.
        ptr = promote(super::head_.next);
        ts = super::policy_.task_size(*ptr);
      }
    }
    return consumed != 0;
  }

  /// Run a new round with `quantum`, dispatching all tasks to `consumer`.
  template <class F>
  bool new_round(deficit_type quantum, F& consumer)
  noexcept(noexcept(consumer(std::declval<value_type&>()))) {
    if (!super::empty()) {
      deficit_ += quantum;
      return consume(consumer);
    }
    return false;
  }

  cache_type& cache() noexcept {
    return cache_;
  }

private:
  // -- member variables ------------------------------------------------------

  /// Stores the deficit on this queue.
  deficit_type deficit_ = 0;

  /// Stores previously skipped items.
  cache_type cache_;
};

} // namespace intrusive
} // namespace caf

#endif // CAF_INTRUSIVE_DRR_CACHED_QUEUE_HPP
