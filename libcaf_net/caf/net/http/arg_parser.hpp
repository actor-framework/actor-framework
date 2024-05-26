// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/parse.hpp"

#include <optional>

namespace caf::net::http {

/// Customization point for adding custom types to the `<arg>` parsing of the
/// @ref router.
template <class>
struct arg_parser;

template <>
struct arg_parser<std::string> {
  std::optional<std::string> parse(std::string_view str) {
    return std::string{str};
  }
};

template <class T>
struct builtin_arg_parser {
  std::optional<T> parse(std::string_view str) {
    auto tmp = T{};
    if (auto err = detail::parse(str, tmp); !err)
      return tmp;
    else
      return std::nullopt;
  }
};

template <class T, bool IsArithmetic = std::is_arithmetic_v<T>>
struct arg_parser_oracle {
  using type = arg_parser<T>;
};

template <class T>
struct arg_parser_oracle<T, true> {
  using type = builtin_arg_parser<T>;
};

template <class T>
using arg_parser_t = typename arg_parser_oracle<T>::type;

} // namespace caf::net::http
