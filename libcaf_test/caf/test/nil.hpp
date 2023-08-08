// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include <string>
#include <vector>

namespace caf::test {

/// A type that can be used to check whether a list is empty.
struct nil_t {};

/// Represents the empty list.
constexpr nil_t nil = nil_t{};

/// @relates nil_t
inline std::string to_string(nil_t) {
  return "nil";
}

/// @relates nil_t
template <class T>
bool operator==(const std::vector<T>& values, nil_t) {
  return values.empty();
}

/// @relates nil_t
template <class T>
bool operator==(nil_t, const T& values) {
  return values == nil;
}

/// @relates nil_t
template <class T>
bool operator!=(const T& values, nil_t) {
  return !(values == nil);
}

/// @relates nil_t
template <class T>
bool operator!=(nil_t, const T& values) {
  return !(values == nil);
}

} // namespace caf::test
