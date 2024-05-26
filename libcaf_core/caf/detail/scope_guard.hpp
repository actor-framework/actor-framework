// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <utility>

namespace caf::detail {

/// A lightweight scope guard implementation.
template <class Fun>
class [[nodiscard]] scope_guard {
public:
  static_assert(noexcept(std::declval<Fun>()()),
                "scope_guard requires a noexcept cleanup function");

  explicit scope_guard(Fun f) noexcept : fun_(std::move(f)), enabled_(true) {
    // nop
  }

  scope_guard() = delete;

  scope_guard(const scope_guard&) = delete;

  scope_guard& operator=(const scope_guard&) = delete;

  ~scope_guard() noexcept {
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

} // namespace caf::detail
