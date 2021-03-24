// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <limits>
#include <utility>

#include "caf/config.hpp"

#include "caf/intrusive/new_round_result.hpp"
#include "caf/intrusive/task_queue.hpp"
#include "caf/intrusive/task_result.hpp"

namespace caf::intrusive {

/// A Deficit Round Robin queue with an internal cache for allowing skipping
/// consumers.
template <class Policy>
class drr_cached_queue { // Note that we do *not* inherit from
                         // task_queue<Policy>, because the cached queue can no
                         // longer offer iterator access.
public:
  // -- member types ----------------------------------------------------------
  using policy_type = Policy;

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
    : list_(p), deficit_(0), cache_(std::move(p)) {
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

  /// Peeks at the first element of the list.
  pointer peek() noexcept {
    return list_.peek();
  }

  /// Applies `f` to each element in the queue, excluding cached elements.
  template <class F>
  void peek_all(F f) const {
    list_.peek_all(f);
  }

  /// Tries to find the next element in the queue (excluding cached elements)
  /// that matches the given predicate.
  template <class Predicate>
  pointer find_if(Predicate pred) {
    return list_.find_if(pred);
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
  /// @private
  unique_pointer next() noexcept {
    return list_.next(deficit_);
  }

  /// Takes the first element out of the queue (after flushing the cache)  and
  /// returns it, ignoring the deficit count.
  unique_pointer take_front() noexcept {
    flush_cache();
    if (!list_.empty()) {
      // Don't modify the deficit counter.
      auto dummy_deficit = std::numeric_limits<deficit_type>::max();
      return list_.next(dummy_deficit);
    }
    return nullptr;
  }

  /// Consumes items from the queue until the queue is empty, there is not
  /// enough deficit to dequeue the next task or the consumer returns `stop`.
  /// @returns `true` if `f` consumed at least one item.
  template <class F>
  bool consume(F& f) noexcept(noexcept(f(std::declval<value_type&>()))) {
    return new_round(0, f).consumed_items > 0;
  }

  /// Run a new round with `quantum`, dispatching all tasks to `consumer`.
  template <class F>
  new_round_result new_round(deficit_type quantum, F& consumer) noexcept(
    noexcept(consumer(std::declval<value_type&>()))) {
    if (list_.empty())
      return {0, false};
    deficit_ += quantum;
    auto ptr = next();
    if (ptr == nullptr)
      return {0, false};
    size_t consumed = 0;
    do {
      auto consumer_res = consumer(*ptr);
      switch (consumer_res) {
        case task_result::skip:
          // Fix deficit counter since we didn't actually use it.
          deficit_ += policy().task_size(*ptr);
          // Push the unconsumed item to the cache.
          cache_.push_back(ptr.release());
          if (list_.empty()) {
            deficit_ = 0;
            return {consumed, false};
          }
          break;
        case task_result::resume:
          ++consumed;
          flush_cache();
          if (list_.empty()) {
            deficit_ = 0;
            return {consumed, false};
          }
          break;
        default:
          ++consumed;
          flush_cache();
          if (list_.empty())
            deficit_ = 0;
          return {consumed, consumer_res == task_result::stop_all};
      }
      ptr = next();
    } while (ptr != nullptr);
    return {consumed, false};
  }

  cache_type& cache() noexcept {
    return cache_;
  }

  list_type& items() noexcept {
    return list_;
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

} // namespace caf::intrusive
