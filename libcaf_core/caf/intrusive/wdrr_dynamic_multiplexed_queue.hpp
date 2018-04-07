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

#include "caf/intrusive/new_round_result.hpp"
#include "caf/intrusive/task_result.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace intrusive {

/// A work queue that internally multiplexes any number of DRR queues.
template <class Policy>
class wdrr_dynamic_multiplexed_queue {
public:
  using policy_type = Policy;

  using deficit_type = typename policy_type::deficit_type;

  using mapped_type = typename policy_type::mapped_type;

  using key_type = typename policy_type::key_type;

  using pointer = mapped_type*;

  using unique_pointer = typename policy_type::unique_pointer;

  using task_size_type = typename policy_type::task_size_type;

  using queue_map_type = typename policy_type::queue_map_type;

  using queue_type = typename queue_map_type::mapped_type;

  template <class... Ps>
  wdrr_dynamic_multiplexed_queue(policy_type p) : policy_(p) {
    // nop
  }

  policy_type& policy() noexcept {
    return policy_;
  }

  const policy_type& policy() const noexcept {
    return policy_;
  }

  bool push_back(mapped_type* ptr) noexcept {
    auto i = qs_.find(policy_.id_of(*ptr));
    if (i != qs_.end()) {
      i->second.push_back(ptr);
      return true;
    } else {
      typename unique_pointer::deleter_type d;
      d(ptr);
      return false;
    }
  }

  bool push_back(unique_pointer ptr) noexcept {
    return push_back(ptr.release());
  }

  template <class... Ts>
  bool emplace_back(Ts&&... xs) {
    return push_back(new mapped_type(std::forward<Ts>(xs)...));
  }

  /// @private
  template <class F>
  struct new_round_helper {
    const key_type& k;
    queue_type& q;
    F& f;
    template <class... Ts>
    auto operator()(Ts&&... xs)
      -> decltype((std::declval<F&>())(std::declval<const key_type&>(),
                                       std::declval<queue_type&>(),
                                       std::forward<Ts>(xs)...)) {
      return f(k, q, std::forward<Ts>(xs)...);
    }
  };

  void inc_deficit(deficit_type x) noexcept {
    for (auto& kvp : qs_) {
      auto& q = kvp.second;
      q.inc_deficit(policy_.quantum(q, x));
    }
  }

  /// Run a new round with `quantum`, dispatching all tasks to `consumer`.
  /// @returns `true` if at least one item was consumed, `false` otherwise.
  template <class F>
  new_round_result new_round(deficit_type quantum, F& f) {
    bool result = false;
    bool stopped = false;
    for (auto& kvp : qs_) {
      if (policy_.enabled(kvp.second)) {
        auto& q = kvp.second;
        if (!stopped) {
          new_round_helper<F> g{kvp.first, q, f};
          auto res = q.new_round(policy_.quantum(q, quantum), g);
          result = res.consumed_items;
          if (res.stop_all)
            stopped = true;
        } else {
          // Always increase deficit, even if a previous queue stopped the
          // consumer preemptively.
          q.inc_deficit(policy_.quantum(q, quantum));
        }
      }
    }
    cleanup();
    return {result, stopped};
  }

  /// Erases all keys previously marked via `erase_later`.
  void cleanup() {
    if (!erase_list_.empty()) {
      for (auto& k : erase_list_)
        qs_.erase(k);
      erase_list_.clear();
    }
  }

  /// Marks the key `k` for erasing from the map later.
  void erase_later(key_type k) {
    erase_list_.emplace_back(std::move(k));
  }

  pointer peek() noexcept {
    for (auto& kvp : qs_) {
      auto ptr = kvp.second.peek();
      if (ptr != nullptr)
        return ptr;
    }
    return nullptr;
  }

  /// Applies `f` to each element in the queue.
  template <class F>
  void peek_all(F f) const {
    for (auto& kvp : qs_)
      kvp.second.peek_all(f);
  }

  /// Returns `true` if all queues are empty, `false` otherwise.
  bool empty() const noexcept {
    return total_task_size() == 0;
  }

  void flush_cache() noexcept {
    for (auto& kvp : qs_)
      kvp.second.flush_cache();
  }

  task_size_type total_task_size() const noexcept {
    task_size_type result = 0;
    for (auto& kvp : qs_)
      if (policy_.enabled(kvp.second))
        result += kvp.second.total_task_size();
    return result;
  }

  /// Returns the tuple containing all nested queues.
  queue_map_type& queues() noexcept {
    return qs_;
  }

  /// Returns the tuple containing all nested queues.
  const queue_map_type& queues() const noexcept {
    return qs_;
  }

  void lifo_append(pointer ptr) noexcept {
    auto i = qs_.find(policy_.id_of(*ptr));
    if (i != qs_.end()) {
      i->second.lifo_append(ptr);
    } else {
      typename unique_pointer::deleter_type d;
      d(ptr);
    }
  }

  void stop_lifo_append() noexcept {
    for (auto& kvp : qs_)
      kvp.second.stop_lifo_append();
  }

private:
  // -- member variables -------------------------------------------------------

  /// All queues.
  queue_map_type qs_;

  /// Policy object for adjusting a quantum based on queue weight etc.
  Policy policy_;

  /// List of keys that are marked erased.
  std::vector<key_type> erase_list_;
};

} // namespace intrusive
} // namespace caf

