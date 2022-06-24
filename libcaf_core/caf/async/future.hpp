// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/execution_context.hpp"
#include "caf/async/fwd.hpp"
#include "caf/detail/async_cell.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/sec.hpp"

namespace caf::async {

/// Provides an interface for accessing the result of an asynchronous
/// computation on an asynchronous @ref execution_context.
template <class T>
class bound_future {
public:
  friend class future<T>;

  bound_future() noexcept = default;
  bound_future(bound_future&&) noexcept = default;
  bound_future(const bound_future&) noexcept = default;
  bound_future& operator=(bound_future&&) noexcept = default;
  bound_future& operator=(const bound_future&) noexcept = default;

  /// Retrieves the result at some point in the future and then calls either
  /// @p on_success  if the asynchronous operation generated a result or
  /// @p on_error if the asynchronous operation resulted in an error.
  template <class OnSuccess, class OnError>
  disposable then(OnSuccess on_success, OnError on_error) {
    static_assert(std::is_invocable_v<OnSuccess, const T&>);
    static_assert(std::is_invocable_v<OnError, const error&>);
    auto cb = [cp = cell_, f = std::move(on_success), g = std::move(on_error)] {
      // Note: no need to lock the mutex. Once the cell has a value and actions
      // are allowed to run, the value is immutable.
      switch (cp->value.index()) {
        default:
          g(make_error(sec::broken_promise, "future found an invalid value"));
          break;
        case 1:
          f(static_cast<const T&>(std::get<T>(cp->value)));
          break;
        case 2:
          g(static_cast<const error&>(std::get<error>(cp->value)));
      }
    };
    auto cb_action = make_single_shot_action(std::move(cb));
    auto event = typename cell_type::event{ctx_, cb_action};
    bool fire_immediately = false;
    { // Critical section.
      std::unique_lock guard{cell_->mtx};
      if (std::holds_alternative<none_t>(cell_->value)) {
        cell_->events.push_back(std::move(event));
      } else {
        fire_immediately = true;
      }
    }
    if (fire_immediately)
      event.first->schedule(std::move(event.second));
    auto res = std::move(cb_action).as_disposable();
    ctx_->watch(res);
    return res;
  }

private:
  using cell_type = detail::async_cell<T>;

  using cell_ptr = std::shared_ptr<cell_type>;

  bound_future(execution_context* ctx, cell_ptr cell)
    : ctx_(ctx), cell_(std::move(cell)) {
    // nop
  }

  execution_context* ctx_;
  cell_ptr cell_;
};

/// Represents the result of an asynchronous computation.
template <class T>
class future {
public:
  friend class promise<T>;

  future() noexcept = default;
  future(future&&) noexcept = default;
  future(const future&) noexcept = default;
  future& operator=(future&&) noexcept = default;
  future& operator=(const future&) noexcept = default;

  bool valid() const noexcept {
    return cell_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  /// Binds this future to an @ref execution_context to run callbacks.
  /// @pre `valid()`
  bound_future<T> bind_to(execution_context* ctx) && {
    return {ctx, std::move(cell_)};
  }

  /// Binds this future to an @ref execution_context to run callbacks.
  /// @pre `valid()`
  bound_future<T> bind_to(execution_context& ctx) && {
    return {&ctx, std::move(cell_)};
  }

  /// Binds this future to an @ref execution_context to run callbacks.
  /// @pre `valid()`
  bound_future<T> bind_to(execution_context* ctx) const& {
    return {ctx, cell_};
  }

  /// Binds this future to an @ref execution_context to run callbacks.
  /// @pre `valid()`
  bound_future<T> bind_to(execution_context& ctx) const& {
    return {&ctx, cell_};
  }

  /// Queries whether the result of the asynchronous computation is still
  /// pending, i.e., neither `set_value` nor `set_error` has been called on the
  /// @ref promise.
  /// @pre `valid()`
  bool pending() const {
    CAF_ASSERT(valid());
    std::unique_lock guard{cell_->mtx};
    return std::holds_alternative<none_t>(cell_->value);
  }

private:
  using cell_ptr = std::shared_ptr<detail::async_cell<T>>;

  explicit future(cell_ptr cell) : cell_(std::move(cell)) {
    // nop
  }

  cell_ptr cell_;
};

} // namespace caf::async
