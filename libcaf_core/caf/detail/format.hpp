// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

// Thin wrapper around std::format. If that is not available, we provide a
// minimal implementation that is "good enough" for our purposes.
//
// TODO: Currently, this only wraps the format_to and format functions as-is.
//       The wrappers should also add support for types that provide an
//       `inspect` overload.

#include "caf/deep_to_string.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/is_complete.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/inspector_access_type.hpp"

#include <string>

#ifdef CAF_USE_STD_FORMAT

#  include <format>

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

#else // here comes the poor man's version

#  include "caf/chunked_string.hpp"
#  include "caf/deep_to_string.hpp"
#  include "caf/detail/core_export.hpp"
#  include "caf/span.hpp"

#  include <array>
#  include <cstdint>
#  include <iterator>
#  include <memory>
#  include <string_view>
#  include <type_traits>
#  include <variant>

namespace caf::detail {

using format_arg
  = std::variant<bool, char, int64_t, uint64_t, double, const char*,
                 std::string_view, chunked_string, std::string, const void*>;

template <class T>
format_arg make_format_arg(const T& arg) {
  if constexpr (is_one_of_v<T, bool, char, const char*, std::string_view>) {
    return format_arg{arg};
  } else if constexpr (std::is_integral_v<T>) {
    if constexpr (std::is_signed_v<T>) {
      return format_arg{static_cast<int64_t>(arg)};
    } else {
      return format_arg{static_cast<uint64_t>(arg)};
    }
  } else if constexpr (std::is_floating_point_v<T>) {
    return format_arg{static_cast<double>(arg)};
  } else if constexpr (std::is_same_v<T, std::string>) {
    return format_arg{std::string_view{arg}};
  } else if constexpr (std::is_same_v<T, chunked_string>) {
    return format_arg{arg};
  } else if constexpr (!std::is_same_v<
                         decltype(inspect_access_type<stringification_inspector,
                                                      T>()),
                         inspector_access_type::none>) {
    return format_arg{deep_to_string(arg)};
  } else {
    static_assert(std::is_pointer_v<T>, "unsupported argument type");
    return format_arg{static_cast<const void*>(arg)};
  }
}

template <size_t N>
format_arg make_format_arg(const char (&arg)[N]) {
  return format_arg{std::string_view{arg, N - 1}};
}

// Interface traversing the formatting output chunk by chunk.
class CAF_CORE_EXPORT compiled_format_string {
public:
  virtual ~compiled_format_string();

  // Checks whether we reached the end of the format string.
  virtual bool at_end() const noexcept = 0;

  // Returns the next chunk of the formatted output.
  virtual std::string_view next() = 0;
};

CAF_CORE_EXPORT
std::unique_ptr<compiled_format_string>
compile_format_string(std::string_view fstr, span<format_arg> args);

template <class OutputIt, class... Args>
auto format_to(OutputIt out, std::string_view fstr, Args&&... raw_args) {
  std::array<format_arg, sizeof...(Args)> args{make_format_arg(raw_args)...};
  auto compiled = compile_format_string(fstr, make_span(args));
  while (!compiled->at_end()) {
    auto chunk = compiled->next();
    out = std::copy(chunk.begin(), chunk.end(), out);
  }
  return out;
}

template <class... Args>
std::string format(std::string_view fstr, Args&&... args) {
  std::string result;
  result.reserve(fstr.size());
  format_to(std::back_inserter(result), fstr, std::forward<Args>(args)...);
  return result;
}

} // namespace caf::detail

#endif
