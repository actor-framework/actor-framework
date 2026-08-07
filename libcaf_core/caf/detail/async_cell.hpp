// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/config.hpp"
#include "caf/detail/atomic_ref_count.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/none.hpp"
#include "caf/sec.hpp"
#include "caf/unit.hpp"

#include <atomic>
#include <mutex>
#include <variant>
#include <vector>

namespace caf::detail {

/// Implementation detail for @ref async::future and @ref async::promise.
template <class T>
class async_cell final : public disposable::impl {
public:
  using value_type = std::conditional_t<std::is_void_v<T>, unit_t, T>;

  static_assert(!std::is_same_v<value_type, error>);

  using state_type = std::variant<none_t, value_type, error>;

  async_cell() : promises_(1) {
    // Make room for a couple of events to avoid frequent heap allocations in
    // critical sections. We could also use a custom allocator to use
    // small-buffer-optimization.
    events_.reserve(8);
  }

  async_cell(const async_cell&) = delete;

  async_cell& operator=(const async_cell&) = delete;

  // Adds a listener to the cell that runs when the cell transitions to a
  // non-pending state, i.e., when setting a value or an error on the cell.
  // @return `true` if the callback was added, `false` if the cell is already
  //         in a non-pending state and no longer accepts callbacks.
  bool subscribe(async::execution_context_ptr ctx, action callback) {
    { // Critical section.
      std::unique_lock guard{mtx_};
      if (std::holds_alternative<none_t>(value_)) {
        events_.emplace_back(std::move(ctx), std::move(callback));
        return true;
      }
    }
    return false;
  }

  /// Increments the promise reference count.
  void inc_promise_ref() noexcept {
    promises_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decrements the promise reference count, breaking the promise if the count
  /// reaches 0 while `value` is still `none`.
  void dec_promise_ref() noexcept {
    if (promises_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      event_list events;
      { // Critical section.
        std::unique_lock guard{mtx_};
        if (std::holds_alternative<none_t>(value_)) {
          value_ = make_error(sec::broken_promise);
          events_.swap(events);
        }
      }
      for (auto& [ctx, callback] : events) {
        if (ctx) {
          ctx->schedule(std::move(callback));
        } else {
          callback.run();
        }
      }
    }
  }

  void ref() const noexcept override {
    ref_count_.inc();
  }

  void deref() const noexcept override {
    ref_count_.dec(this);
  }

  void dispose() override {
    event on_dispose;
    event_list events;
    {
      std::unique_lock guard{mtx_};
      if (!std::holds_alternative<none_t>(value_)) {
        return;
      }
      value_ = make_error(sec::disposed);
      events_.swap(events);
      std::swap(on_dispose, on_dispose_);
    }
    for (auto& [ctx, callback] : events) {
      if (ctx) {
        ctx->schedule(std::move(callback));
      } else {
        callback.run();
      }
    }
    auto& [ctx, callback] = on_dispose;
    if (callback) {
      if (ctx) {
        ctx->schedule(std::move(callback));
      } else {
        callback.run();
      }
    }
  }

  bool disposed() const noexcept override {
    std::unique_lock guard{mtx_};
    return !std::holds_alternative<none_t>(value_);
  }

  /// Tries to set the dispose callback.
  /// @return `true` if the callback was set, `false` if the cell is already
  ///         disposed.
  bool set_on_dispose(async::execution_context_ptr ctx, action callback) {
    {
      std::unique_lock guard{mtx_};
      if (!std::holds_alternative<none_t>(value_)) {
        return false;
      }
      std::swap(on_dispose_.first, ctx);
      std::swap(on_dispose_.second, callback);
    }
    return true;
  }

  /// Sets the value of the cell. The first write wins, subsequent writes are
  /// ignored.
  template <class What>
    requires one_of<std::remove_cvref_t<What>, value_type, error>
  void set(What&& what) {
    event_list events;
    { // Critical section.
      std::unique_lock guard{mtx_};
      if (!std::holds_alternative<none_t>(value_)) {
        return;
      }
      value_ = std::forward<What>(what);
      events_.swap(events);
    }
    for (auto& [ctx, callback] : events) {
      if (ctx)
        ctx->schedule(std::move(callback));
      else
        callback.run();
    }
  }

  /// Visits the value of the cell using the given visitor.
  /// @note Holds the mutex while visiting the value. To run non-trivial code on
  ///       the value, copy the value out of the cell first via `get()`.
  template <class Visitor>
  auto visit(Visitor&& visitor) const {
    std::unique_lock guard{mtx_};
    return std::visit(std::forward<Visitor>(visitor), value_);
  }

  /// Returns the current state of the cell.
  state_type get() const {
    std::unique_lock guard{mtx_};
    return value_;
  }

private:
  using event = std::pair<async::execution_context_ptr, action>;

  using event_list = std::vector<event>;

  /// The intrusive reference count.
  mutable atomic_ref_count ref_count_;

  /// The number of promises that currently hold a reference to the cell. If
  /// this reaches 0 while the cell is still `none`, we set an error to inform
  /// all futures that the promise has been broken.
  std::atomic<size_t> promises_;

  /// Protects all subsequent member variables.
  alignas(CAF_CACHE_LINE_SIZE) mutable std::mutex mtx_;

  /// The value of the cell.
  state_type value_;

  /// The callback events of the subscribers.
  event_list events_;

  /// Optional callback to dispose the task that should produce the result.
  event on_dispose_;
};

} // namespace caf::detail
