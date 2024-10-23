// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/behavior_impl.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/none.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/timespan.hpp"
#include "caf/unsafe_behavior_init.hpp"

#include <functional>
#include <type_traits>

namespace caf {

class message_handler;

/// Describes the behavior of an actor, i.e., provides a message
/// handler and an optional timeout.
class CAF_CORE_EXPORT behavior {
public:
  friend class message_handler;

  behavior() = default;
  behavior(behavior&&) = default;
  behavior(const behavior&) = default;
  behavior& operator=(behavior&&) = default;
  behavior& operator=(const behavior&) = default;

  // Convenience overload to allow "unsafe" initialization of any behavior_type.
  behavior(unsafe_behavior_init_t, behavior from) : behavior(std::move(from)) {
    // nop
  }

  /// Creates a behavior from `fun` without timeout.
  behavior(const message_handler& mh);

  /// The list of arguments can contain match expressions, message handlers,
  /// and up to one timeout (if set, the timeout has to be the last argument).
  template <class T, class... Ts>
  behavior(T arg, Ts&&... args) {
    if constexpr (is_timeout_definition_v<T>
                  || (is_timeout_definition_v<Ts> || ...)) {
      legacy_assign(std::move(arg), std::forward<Ts>(args)...);
    } else {
      impl_ = detail::make_behavior(std::move(arg), std::forward<Ts>(args)...);
    }
  }

  /// Creates a behavior from `tdef` without message handler.
  template <class F>
  [[deprecated("use idle timeouts instead of 'after >> ...'")]]
  behavior(timeout_definition<F> tdef)
    : impl_(detail::make_behavior(tdef)) {
    // nop
  }

  /// Assigns new handlers.
  template <class... Ts>
  [[deprecated("use idle timeouts instead of 'after >> ...'")]]
  void legacy_assign(Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "assign() called without arguments");
    impl_ = detail::make_behavior(std::forward<Ts>(xs)...);
  }

  /// Assigns new handlers.
  template <class T, class... Ts>
  void assign(T&& arg, Ts&&... args) {
    if constexpr (is_timeout_definition_v<T>
                  || (is_timeout_definition_v<Ts> || ...)) {
      legacy_assign(std::forward<T>(arg), std::forward<Ts>(args)...);
    } else {
      impl_ = detail::make_behavior(std::forward<T>(arg),
                                    std::forward<Ts>(args)...);
    }
  }

  void swap(behavior& other) {
    impl_.swap(other.impl_);
  }

  void assign(intrusive_ptr<detail::behavior_impl> ptr) {
    impl_.swap(ptr);
  }

  /// Equal to `*this = other`.
  void assign(message_handler other);

  /// Equal to `*this = other`.
  void assign(behavior other);

  /// Invokes the timeout callback if set.
  void handle_timeout() {
    impl_->handle_timeout();
  }

  /// Returns the timespan after which receive operations
  /// using this behavior should time out.
  timespan timeout() const noexcept {
    return impl_->timeout();
  }

  /// Runs this handler and returns its (optional) result.
  std::optional<message> operator()(message& xs) {
    return impl_ ? impl_->invoke(xs) : std::nullopt;
  }

  /// Runs this handler with callback.
  bool operator()(detail::invoke_result_visitor& f, message& xs) {
    return impl_ ? impl_->invoke(f, xs) : false;
  }

  /// Checks whether this behavior is not empty.
  operator bool() const {
    return static_cast<bool>(impl_);
  }

  /// @cond

  using impl_ptr = intrusive_ptr<detail::behavior_impl>;

  const impl_ptr& as_behavior_impl() const {
    return impl_;
  }

  behavior(impl_ptr ptr) : impl_(std::move(ptr)) {
    // nop
  }

  behavior& unbox() & {
    return *this;
  }

  behavior&& unbox() && {
    return std::move(*this);
  }

  /// @endcond

private:
  impl_ptr impl_;
};

} // namespace caf
