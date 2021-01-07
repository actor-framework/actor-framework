// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>
#include <tuple>

#include "caf/detail/stringification_inspector.hpp"

namespace caf {

/// Unrolls collections such as vectors/maps, decomposes
/// tuples/pairs/arrays, auto-escapes strings and calls
/// `to_string` for user-defined types via argument-dependent
/// loopkup (ADL). Any user-defined type that does not
/// provide a `to_string` is mapped to `<unprintable>`.
template <class T>
std::string deep_to_string(const T& x) {
  using inspector_type = detail::stringification_inspector;
  std::string result;
  inspector_type f{result};
  detail::save(f, detail::as_mutable_ref(x));
  return result;
}

/// Convenience function for `deep_to_string(std::forward_as_tuple(xs...))`.
template <class... Ts>
std::string deep_to_string_as_tuple(const Ts&... xs) {
  return deep_to_string(std::forward_as_tuple(xs...));
}

/// Wraps `deep_to_string` into a function object.
struct deep_to_string_t {
  template <class... Ts>
  std::string operator()(const Ts&... xs) const {
    return deep_to_string(xs...);
  }
};

} // namespace caf
