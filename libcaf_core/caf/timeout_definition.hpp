// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <functional>
#include <type_traits>

#include "caf/detail/core_export.hpp"
#include "caf/timespan.hpp"

namespace caf {

namespace detail {

class behavior_impl;

CAF_CORE_EXPORT behavior_impl* new_default_behavior(timespan d,
                                                    std::function<void()> fun);

} // namespace detail

template <class F>
struct timeout_definition {
  static constexpr bool may_have_timeout = true;

  timespan timeout;

  F handler;

  detail::behavior_impl* as_behavior_impl() const {
    return detail::new_default_behavior(timeout, handler);
  }

  timeout_definition() = default;
  timeout_definition(timeout_definition&&) = default;
  timeout_definition(const timeout_definition&) = default;

  timeout_definition(timespan timeout, F&& f)
    : timeout(timeout), handler(std::move(f)) {
    // nop
  }

  template <class U>
  timeout_definition(const timeout_definition<U>& other)
    : timeout(other.timeout), handler(other.handler) {
    // nop
  }
};

template <class T>
struct is_timeout_definition : std::false_type {};

template <class T>
struct is_timeout_definition<timeout_definition<T>> : std::true_type {};

using generic_timeout_definition = timeout_definition<std::function<void()>>;

} // namespace caf
