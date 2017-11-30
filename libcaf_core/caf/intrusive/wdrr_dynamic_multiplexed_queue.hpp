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

#ifndef CAF_INTRUSIVE_WDRR_DYNAMIC_MULTIPLEXED_QUEUE_HPP
#define CAF_INTRUSIVE_WDRR_DYNAMIC_MULTIPLEXED_QUEUE_HPP

#include <utility>

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace intrusive {

/// A work queue that internally multiplexes any number of DRR queues.
template <class Policy>
class wdrr_dynamic_multiplexed_queue {
public:
  using policy_type = Policy;

  using deleter_type = typename policy_type::deleter_type;

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

  void push_back(mapped_type* ptr) noexcept {
    auto i = qs_.find(policy_.id_of(*ptr));
    if (i != qs_.end()) {
      i->second.push_back(ptr);
    } else {
      deleter_type d;
      d(ptr);
    }
  }

  void push_back(unique_pointer ptr) noexcept {
    push_back(ptr.release());
  }

  template <class... Ts>
  void emplace_back(Ts&&... xs) {
    push_back(new mapped_type(std::forward<Ts>(xs)...));
  }

  /// Run a new round with `quantum`, dispatching all tasks to `consumer`.
  /// @returns `true` if at least one item was consumed, `false` otherwise.
  template <class F>
  bool new_round(long quantum,
                 F& f) noexcept(noexcept(f(std::declval<const key_type&>(),
                                           std::declval<queue_type&>(),
                                           std::declval<mapped_type&>()))) {
    bool result = false;
    for (auto& kvp : qs_) {
      auto g = [&](mapped_type& x) {
        f(kvp.first, kvp.second, x);
      };
      result |= kvp.second.new_round(policy_.quantum(kvp.second, quantum), g);
    }
    return result;
  }

  pointer peek() noexcept {
    for (auto& kvp : qs_) {
      auto ptr = kvp.second.peek();
      if (ptr != nullptr)
        return ptr;
    }
    return nullptr;
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
      deleter_type d;
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
};

} // namespace intrusive
} // namespace caf

#endif // CAF_INTRUSIVE_WDRR_DYNAMIC_MULTIPLEXED_QUEUE_HPP
