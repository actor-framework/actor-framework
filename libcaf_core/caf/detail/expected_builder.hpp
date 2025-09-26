// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/expected.hpp"

namespace caf::detail {

template <class... Ts>
struct expected_builder;

template <>
struct expected_builder<> {
  expected<void> result;
  void set_value() {
    // nop
  }
  void set_error(error x) {
    result = std::move(x);
  }
};

template <class T>
struct expected_builder<T> {
  expected<T> result;
  expected_builder() : result(T{}) {
    // nop
  }
  void set_value(T value) {
    result.emplace(std::move(value));
  }
  void set_error(error x) {
    result = std::move(x);
  }
};

template <class T1, class T2, class... Ts>
struct expected_builder<T1, T2, Ts...> {
  expected<std::tuple<T1, T2, Ts...>> result;
  expected_builder() : result(std::tuple{T1{}, T2{}, Ts{}...}) {
    // nop
  }
  void set_value(T1 arg1, T2 arg2, Ts... args) {
    result = std::tuple{std::move(arg1), std::move(arg2), std::move(args)...};
  }
  void set_error(error x) {
    result = std::move(x);
  }
};

} // namespace caf::detail
