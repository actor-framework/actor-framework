// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/message.hpp"
#include "caf/result.hpp"

#include <functional>
#include <type_traits>

namespace caf {

template <class F>
struct catch_all {
  using fun_type = std::function<skippable_result(message&)>;

  static_assert(std::is_convertible_v<F, fun_type>,
                "catch-all handler must have signature "
                "skippable_result (message&)");

  F handler;

  catch_all(catch_all&& x) = default;

  template <class T, class = std::enable_if_t<
                       !std::is_same_v<std::decay_t<T>, catch_all>>>
  explicit catch_all(T&& x) : handler(std::forward<T>(x)) {
    // nop
  }

  fun_type lift() const {
    return handler;
  }
};

template <class T>
struct is_catch_all : std::false_type {};

template <class T>
struct is_catch_all<catch_all<T>> : std::true_type {};

} // namespace caf
