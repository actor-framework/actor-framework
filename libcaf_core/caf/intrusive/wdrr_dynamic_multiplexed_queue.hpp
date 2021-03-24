// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <utility>

#include "caf/intrusive/new_round_result.hpp"
#include "caf/intrusive/task_result.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf::intrusive {

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

  ~wdrr_dynamic_multiplexed_queue() noexcept {
    for (auto& kvp : qs_)
      policy_.cleanup(kvp.second);
  }

  policy_type& policy() noexcept {
    return policy_;
  }

  const policy_type& policy() const noexcept {
    return policy_;
  }

  bool push_back(mapped_type* ptr) noexcept {
    if (auto i = qs_.find(policy_.id_of(*ptr)); i != qs_.end()) {
      return policy_.push_back(i->second, ptr);
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
    size_t consumed = 0;
    bool stopped = false;
    for (auto& kvp : qs_) {
      if (policy_.enabled(kvp.second)) {
        auto& q = kvp.second;
        if (!stopped) {
          new_round_helper<F> g{kvp.first, q, f};
          auto res = q.new_round(policy_.quantum(q, quantum), g);
          consumed += res.consumed_items;
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
    return {consumed, stopped};
  }

  /// Erases all keys previously marked via `erase_later`.
  void cleanup() {
    if (!erase_list_.empty()) {
      for (auto& k : erase_list_) {
        if (auto i = qs_.find(k); i != qs_.end()) {
          policy_.cleanup(i->second);
          qs_.erase(i);
        }
      }
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

  template <class Predicate>
  pointer find_if(Predicate pred) {
    for (auto& kvp : qs_)
      if (auto ptr = kvp.second.find_if(pred))
        return ptr;
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
    if (auto i = qs_.find(policy_.id_of(*ptr)); i != qs_.end()) {
      policy_.lifo_append(i->second, ptr);
    } else {
      typename unique_pointer::deleter_type d;
      d(ptr);
    }
  }

  void stop_lifo_append() noexcept {
    for (auto& kvp : qs_)
      policy_.stop_lifo_append(kvp.second);
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

} // namespace caf::intrusive
