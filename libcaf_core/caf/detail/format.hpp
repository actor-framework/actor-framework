// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

// Thin wrapper around std::format.
//
// TODO: Currently, this only wraps the format_to and format functions as-is.
//       The wrappers should also add support for types that provide an
//       `inspect` overload.

#include "caf/deep_to_string.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/inspector_access_type.hpp"

#include <format>
#include <string>

namespace caf::detail {

template <class T>
decltype(auto) fmt_fwd(T&& arg) {
  using arg_t = std::decay_t<T>;
  if constexpr (std::is_default_constructible_v<std::formatter<arg_t, char>>) {
    return std::forward<T>(arg);
  } else {
    static_assert(
      !std::is_same_v<
        decltype(inspect_access_type<stringification_inspector, arg_t>()),
        inspector_access_type::none>,
      "stringification using std::formatter or caf::inspect not available");
    return deep_to_string(arg);
  }
}

template <class OutputIt, class... Args>
auto format_to(OutputIt out, std::string_view fstr, Args&&... args) {
  // Note: make_format_args expects all args to be references, and since
  // fmt_fwd returns by value in case of inspector stringification, we need this
  // helper function to wrap the call.
  auto format_to_helper = [&out, &fstr](const auto&... ts) {
    return std::vformat_to(out, fstr, std::make_format_args(ts...));
  };
  return format_to_helper(fmt_fwd(args)...);
}

template <class... Args>
std::string format(std::string_view fstr, Args&&... args) {
  // Note: make_format_args expects all args to be references, and since
  // fmt_fwd returns by value in case of inspector stringification, we need this
  // helper function to wrap the call.
  auto format_helper = [&fstr](const auto&... ts) {
    return std::vformat(fstr, std::make_format_args(ts...));
  };
  return format_helper(fmt_fwd(args)...);
}

} // namespace caf::detail
