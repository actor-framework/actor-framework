// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/concepts.hpp"
#include "caf/predicate.hpp"

#include <functional>
#include <memory>

namespace caf {

/// Describes a predicate that allows rebinding to a different implementation.
/// A default-constructed `rebindable_predicate` never matches any value.
template <class... Args>
class rebindable_predicate : public predicate<Args...> {
public:
  using pointer = std::shared_ptr<predicate<Args...>>;

  rebindable_predicate() = default;

  rebindable_predicate(rebindable_predicate&&) noexcept = default;

  rebindable_predicate(const rebindable_predicate&) noexcept = default;

  rebindable_predicate& operator=(rebindable_predicate&&) = default;

  rebindable_predicate& operator=(const rebindable_predicate&) noexcept
    = default;

  explicit rebindable_predicate(pointer ptr) noexcept : ptr_(std::move(ptr)) {
    // nop
  }

  template <class Fun>
    requires(!std::is_same_v<std::decay_t<Fun>, rebindable_predicate>
             && std::is_invocable_r_v<bool, Fun, Args...>)
  explicit rebindable_predicate(Fun&& fun) {
    assign(std::forward<Fun>(fun));
  }

  /// Creates a rebindable predicate that matches any value.
  static rebindable_predicate from(decltype(std::ignore)) {
    return rebindable_predicate{[](Args...) { return true; }};
  }

  /// Creates a rebindable predicate that matches any value and stores the
  /// values in the reference wrappers.
  static rebindable_predicate
  from(std::reference_wrapper<std::decay_t<Args>>... refs) {
    return rebindable_predicate{[refs...](Args... found) {
      (static_cast<void>(refs.get() = found), ...);
      return true;
    }};
  }

  /// Creates a rebindable predicate from a function object.
  template <class Fun>
    requires(!std::is_same_v<std::decay_t<Fun>, rebindable_predicate>
             && std::is_invocable_r_v<bool, Fun, Args...>)
  static rebindable_predicate from(Fun&& fun) {
    return rebindable_predicate{std::forward<Fun>(fun)};
  }

  /// Creates a rebindable predicate that checks whether
  /// the found values are equal to the given expected values.
  template <class... Values>
    requires(detail::is_comparable<Args, Values> && ...)
  static rebindable_predicate from(Values... lhs) {
    return rebindable_predicate{
      [lhs...](Args... rhs) { return ((lhs == rhs) && ...); }};
  }

  template <class Fun>
    requires std::is_invocable_r_v<bool, Fun, Args...>
  rebindable_predicate& operator=(Fun&& fun) {
    assign(std::forward<Fun>(fun));
    return *this;
  }

  bool operator()(Args... args) const final {
    if (ptr_) {
      return ptr_->operator()(args...);
    }
    return false;
  }

private:
  template <class Fun>
  void assign(Fun&& fun) {
    using fun_t = std::decay_t<Fun>;
    using wrapper_t = detail::predicate_wrapper_impl<fun_t, Args...>;
    ptr_ = std::make_shared<wrapper_t>(std::forward<Fun>(fun));
  }

  pointer ptr_;
};

} // namespace caf
