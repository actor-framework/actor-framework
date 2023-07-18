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

  static_assert(std::is_convertible<F, fun_type>::value,
                "catch-all handler must have signature "
                "skippable_result (message&)");

  F handler;

  catch_all(catch_all&& x) : handler(std::move(x.handler)) {
    // nop
  }

  template <class T>
  catch_all(T&& x) : handler(std::forward<T>(x)) {
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
