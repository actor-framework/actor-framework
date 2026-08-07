// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_clock.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/async/fwd.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/async_cell.hpp"
#include "caf/detail/beacon.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/sec.hpp"

namespace caf::async {

/// Provides an interface for accessing the result of an asynchronous
/// computation on an asynchronous @ref execution_context.
template <class T>
class bound_future final {
public:
  friend class future<T>;

  bound_future() noexcept = default;

  /// Retrieves the result at some point in the future and then calls either
  /// @p on_success  if the asynchronous operation generated a result or
  /// @p on_error if the asynchronous operation resulted in an error.
  template <class OnSuccess, class OnError>
  disposable then(OnSuccess on_success, OnError on_error) {
    static_assert(std::is_invocable_v<OnSuccess, const T&>);
    static_assert(std::is_invocable_v<OnError, const error&>);
    auto cb = [cp = cell_, f = std::move(on_success),
               g = std::move(on_error)]() mutable {
      std::visit(
        [&f, &g]<class Inner>([[maybe_unused]] const Inner& val) {
          if constexpr (std::is_same_v<Inner, none_t>) {
            auto err = make_error(sec::broken_promise,
                                  "future found an invalid value");
            g(err);
          } else if constexpr (std::is_same_v<Inner, error>) {
            g(val);
          } else {
            if constexpr (std::is_void_v<T>) {
              f();
            } else {
              f(val);
            }
          }
        },
        cp->get());
    };
    auto cb_action = make_single_shot_action(std::move(cb));
    if (!cell_->subscribe({ctx_, add_ref}, cb_action))
      ctx_->schedule(cb_action);
    auto res = std::move(cb_action).as_disposable();
    ctx_->watch(res);
    return res;
  }

private:
  using cell_type = detail::async_cell<T>;

  using cell_ptr = intrusive_ptr<cell_type>;

  bound_future(execution_context* ctx, cell_ptr cell)
    : ctx_(ctx), cell_(std::move(cell)) {
    // nop
  }

  execution_context* ctx_;
  cell_ptr cell_;
};

/// Represents the result of an asynchronous computation.
template <class T>
class future final {
  using res_t = expected<T>;

public:
  friend class promise<T>;

  future() noexcept = default;

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

  /// Binds this future to a @ref flow::coordinator and converts it to an
  /// @ref flow::observable.
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
    const auto is_none = []<class Inner>(const Inner&) {
      return std::is_same_v<Inner, none_t>;
    };
    return cell_->visit(is_none);
  }

  auto get() const {
    auto sync = make_counted<detail::beacon>();
    if (cell_->subscribe(nullptr, action{sync})) {
      std::ignore = sync->wait();
    }
    return cell_->visit([]<class Inner>([[maybe_unused]] const Inner& val) {
      if constexpr (std::is_same_v<Inner, none_t>) {
        return res_t{unexpect, sec::broken_promise};
      } else if constexpr (std::is_same_v<Inner, error>) {
        return res_t{unexpect, val};
      } else {
        if constexpr (std::is_void_v<T>) {
          return res_t{};
        } else {
          return res_t{val};
        }
      }
    });
  }

  template <class Clock, class Duration>
  auto get(std::chrono::time_point<Clock, Duration> timepoint) const {
    auto sync = make_counted<detail::beacon>();
    if (cell_->subscribe(nullptr, action{sync})) {
      std::ignore = sync->wait_until(timepoint);
    }
    return cell_->visit([]<class Inner>([[maybe_unused]] const Inner& val) {
      if constexpr (std::is_same_v<Inner, none_t>) {
        return res_t{unexpect, sec::future_timeout};
      } else if constexpr (std::is_same_v<Inner, error>) {
        return res_t{unexpect, val};
      } else {
        if constexpr (std::is_void_v<T>) {
          return res_t{};
        } else {
          return res_t{val};
        }
      }
    });
  }

  template <class Rep, class Period>
  auto get(std::chrono::duration<Rep, Period> timeout) const {
    return get(std::chrono::steady_clock::now() + timeout);
  }

  /// Aborts the asynchronous computation. Any future pointing to the same
  /// result will observe a `sec::disposed` error. Whether any running
  /// background activity stops is implementation-dependent.
  void dispose() {
    cell_->dispose();
  }

  /// Converts this future to a @ref disposable.
  disposable as_disposable() const noexcept {
    return disposable{cell_};
  }

private:
  using cell_ptr = intrusive_ptr<detail::async_cell<T>>;

  explicit future(cell_ptr cell) noexcept : cell_(std::move(cell)) {
    // nop
  }

  cell_ptr cell_;
};

} // namespace caf::async
