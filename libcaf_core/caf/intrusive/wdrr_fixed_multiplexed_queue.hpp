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

#ifndef CAF_INTRUSIVE_WDRR_FIXED_MULTIPLEXED_QUEUE_HPP
#define CAF_INTRUSIVE_WDRR_FIXED_MULTIPLEXED_QUEUE_HPP

#include <tuple>
#include <utility>

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace intrusive {

/// A work queue that internally multiplexes any number of DRR queues.
template <class Policy, class Q, class... Qs>
class wdrr_fixed_multiplexed_queue {
public:
  using tuple_type = std::tuple<Q, Qs...>;

  using policy_type = Policy;

  using deficit_type = typename policy_type::deficit_type;

  using mapped_type = typename policy_type::mapped_type;

  using pointer = mapped_type*;

  using unique_pointer = typename policy_type::unique_pointer;

  using task_size_type = typename policy_type::task_size_type;

  static constexpr size_t num_queues = sizeof...(Qs) + 1;

  template <class... Ps>
  wdrr_fixed_multiplexed_queue(policy_type p0,
                               typename Q::policy_type p1, Ps... ps)
      : qs_(std::move(p1), std::forward<Ps>(ps)...),
        policy_(std::move(p0)) {
    // nop
  }

  policy_type& policy() noexcept {
    return policy_;
  }

  const policy_type& policy() const noexcept {
    return policy_;
  }

  void push_back(mapped_type* ptr) noexcept {
    push_back_recursion<0>(policy_.id_of(*ptr), ptr);
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
                 F& f) noexcept(noexcept(f(std::declval<mapped_type&>()))) {
    return new_round_recursion<0>(quantum, f) != 0;
  }

  pointer peek() noexcept {
    return peek_recursion<0>();
  }

  /// Returns `true` if all queues are empty, `false` otherwise.
  bool empty() const noexcept {
    return total_task_size() == 0;
  }

  void flush_cache() noexcept {
    flush_cache_recursion<0>();
  }

  task_size_type total_task_size() const noexcept {
    return total_task_size_recursion<0>();
  }

  /// Returns the tuple containing all nested queues.
  tuple_type& queues() noexcept {
    return qs_;
  }

  /// Returns the tuple containing all nested queues.
  const tuple_type& queues() const noexcept {
    return qs_;
  }

  void lifo_append(pointer ptr) noexcept {
    lifo_append_recursion<0>(policy_.id_of(*ptr), ptr);
  }

  void stop_lifo_append() noexcept {
    stop_lifo_append_recursion<0>();
  }

private:
  // -- recursive helper functions ---------------------------------------------

  template <size_t I>
  detail::enable_if_t<I == num_queues>
  push_back_recursion(size_t, mapped_type*) noexcept {
    // nop
  }

  template <size_t I>
  detail::enable_if_t<I != num_queues>
  push_back_recursion(size_t pos, mapped_type* ptr) noexcept {
    if (pos == I) {
      auto& q = std::get<I>(qs_);
      q.push_back(ptr);
    } else {
      push_back_recursion<I + 1>(pos, ptr);
    }
  }

  template <size_t I, class F>
  detail::enable_if_t<I == num_queues, int>
  new_round_recursion(deficit_type, F&) noexcept {
    return 0;
  }

  template <size_t I, class F>
  detail::enable_if_t<I != num_queues, int>
  new_round_recursion(deficit_type quantum,
                  F& f) noexcept(noexcept(f(std::declval<mapped_type&>()))) {
    auto& q = std::get<I>(qs_);
    if (q.new_round(policy_.quantum(q, quantum), f))
      return 1 + new_round_recursion<I + 1>(quantum, f);
    return 0 + new_round_recursion<I + 1>(quantum, f);
  }

  template <size_t I>
  detail::enable_if_t<I == num_queues, pointer> peek_recursion() noexcept {
    return nullptr;
  }

  template <size_t I>
  detail::enable_if_t<I != num_queues, pointer> peek_recursion() noexcept {
    auto ptr = std::get<I>(qs_).peek();
    if (ptr != nullptr)
      return ptr;
    return peek_recursion<I + 1>();
  }

  template <size_t I>
  detail::enable_if_t<I == num_queues> flush_cache_recursion() noexcept {
    // nop
  }

  template <size_t I>
  detail::enable_if_t<I != num_queues> flush_cache_recursion() noexcept {
    std::get<I>(qs_).flush_cache();
    flush_cache_recursion<I + 1>();
  }
  template <size_t I>
  detail::enable_if_t<I == num_queues, task_size_type>
  total_task_size_recursion() const noexcept {
    return 0;
  }

  template <size_t I>
  detail::enable_if_t<I != num_queues, task_size_type>
  total_task_size_recursion() const noexcept {
    return std::get<I>(qs_).total_task_size() + total_task_size_recursion<I + 1>();
  }

  template <size_t I>
  detail::enable_if_t<I == num_queues>
  lifo_append_recursion(size_t, pointer) noexcept {
    // nop
  }

  template <size_t I>
  detail::enable_if_t<I != num_queues>
  lifo_append_recursion(size_t i, pointer ptr) noexcept {
    if (i == I)
      std::get<I>(qs_).lifo_append(ptr);
    else
      lifo_append_recursion<I + 1>(i, ptr);
  }

  template <size_t I>
  detail::enable_if_t<I == num_queues> stop_lifo_append_recursion() noexcept {
    // nop
  }

  template <size_t I>
  detail::enable_if_t<I != num_queues> stop_lifo_append_recursion() noexcept {
    std::get<I>(qs_).stop_lifo_append();
    stop_lifo_append_recursion<I + 1>();
  }

  // -- member variables -------------------------------------------------------

  /// All queues.
  tuple_type qs_;

  /// Policy object for adjusting a quantum based on queue weight etc.
  Policy policy_;
};

} // namespace intrusive
} // namespace caf

#endif // CAF_INTRUSIVE_WDRR_FIXED_MULTIPLEXED_QUEUE_HPP
