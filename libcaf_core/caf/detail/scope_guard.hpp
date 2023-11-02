// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <utility>

namespace caf::detail {

/// A lightweight scope guard implementation.
template <class Fun>
class scope_guard {
  static_assert(noexcept(std::declval<Fun>()()),
                "Scope guard function must be declared noexcept");
  scope_guard() = delete;
  scope_guard(const scope_guard&) = delete;
  scope_guard& operator=(const scope_guard&) = delete;

public:
  scope_guard(Fun f) : fun_(std::move(f)), enabled_(true) {
  }

  scope_guard(scope_guard&& other)
    : fun_(std::move(other.fun_)), enabled_(other.enabled_) {
    other.enabled_ = false;
  }

  ~scope_guard() {
    if (enabled_)
      fun_();
  }

  /// Disables this guard, i.e., the guard does not
  /// run its cleanup code as it goes out of scope.
  void disable() noexcept {
    enabled_ = false;
  }

private:
  Fun fun_;
  bool enabled_;
};

/// Creates a guard that executes `f` as soon as it goes out of scope.
/// @relates scope_guard
template <class Fun>
scope_guard<Fun> make_scope_guard(Fun f) {
  return {std::move(f)};
}

} // namespace caf::detail
