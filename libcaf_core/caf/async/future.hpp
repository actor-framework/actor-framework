// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/execution_context.hpp"
#include "caf/async/fwd.hpp"
#include "caf/detail/async_cell.hpp"
#include "caf/detail/beacon.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/op/cell.hpp"
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
    auto cb = [cp = cell_, f = std::move(on_success),
               g = std::move(on_error)]() mutable {
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
    if (!cell_->subscribe(ctx_, cb_action))
      ctx_->schedule(cb_action);
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

  /// Binds this future to a @ref coordinator and converts it to an
  /// @ref observable.
  /// @pre `valid()`
  flow::observable<T> observe_on(flow::coordinator* ctx) const {
    using flow_cell_t = flow::op::cell<T>;
    auto ptr = make_counted<flow_cell_t>(ctx);
    bind_to(ctx).then([ptr](const T& val) { ptr->set_value(val); },
                      [ptr](const error& what) { ptr->set_error(what); });
    return flow::observable<T>{ptr};
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

  auto get() {
    using res_t = expected<T>;
    auto sync = make_counted<detail::beacon>();
    if (cell_->subscribe(nullptr, action{sync})) {
      std::ignore = sync->wait();
    }
    std::unique_lock guard{cell_->mtx};
    switch (cell_->value.index()) {
      default:
        return res_t{sec::broken_promise};
      case 1:
        if constexpr (std::is_void_v<T>)
          return res_t{};
        else
          return res_t{std::get<T>(cell_->value)};
      case 2:
        return res_t{std::get<error>(cell_->value)};
    }
  }

private:
  using cell_ptr = std::shared_ptr<detail::async_cell<T>>;

  explicit future(cell_ptr cell) : cell_(std::move(cell)) {
    // nop
  }

  cell_ptr cell_;
};

} // namespace caf::async
