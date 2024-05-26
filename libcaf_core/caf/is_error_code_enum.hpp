// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/pp.hpp"

namespace caf {

/// Customization point for enabling conversion from an enum type to an
/// @ref error or @ref error_code.
template <class T>
struct is_error_code_enum {
  static constexpr bool value = false;
};

/// @relates is_error_code_enum
template <class T>
constexpr bool is_error_code_enum_v = is_error_code_enum<T>::value;

} // namespace caf

#define CAF_ERROR_CODE_ENUM(type_name)                                         \
  namespace caf {                                                              \
  template <>                                                                  \
  struct is_error_code_enum<type_name> {                                       \
    static constexpr bool value = true;                                        \
  };                                                                           \
  }
