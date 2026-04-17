// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <utility>

namespace caf {

/// Describes a simple predicate, usually implemented via lambda expression.
/// Unlike a `callback`, a `predicate` always returns a `bool` value.
template <class... Args>
class predicate {
public:
  virtual ~predicate() {
    // nop
  }

  virtual bool operator()(Args... args) const = 0;
};

} // namespace caf

namespace caf::detail {

/// Utility class for wrapping a function object of type `F` by reference.
template <class F, class... Args>
class predicate_ref_impl final : public predicate<Args...> {
public:
  explicit predicate_ref_impl(const F& f) : f_(&f) {
    // nop
  }

  predicate_ref_impl(predicate_ref_impl&&) = default;

  predicate_ref_impl& operator=(predicate_ref_impl&&) = default;

  bool operator()(Args... args) const override {
    return (*f_)(std::forward<Args>(args)...);
  }

private:
  const F* f_;
};

/// Utility class for wrapping a function object of type `F` by value.
template <class F, class... Args>
class predicate_wrapper_impl final : public predicate<Args...> {
public:
  template <class... CtorArgs>
    requires std::is_constructible_v<F, CtorArgs...>
  explicit predicate_wrapper_impl(CtorArgs&&... args)
    : f_(std::forward<CtorArgs>(args)...) {
    // nop
  }

  predicate_wrapper_impl(const predicate_wrapper_impl&) = delete;

  predicate_wrapper_impl& operator=(const predicate_wrapper_impl&) = delete;

  bool operator()(Args... args) const override {
    return f_(std::forward<Args>(args)...);
  }

private:
  F f_;
};

} // namespace caf::detail
