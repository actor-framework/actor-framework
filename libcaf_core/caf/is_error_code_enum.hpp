/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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

// TODO: the overload with two arguments exists only for backwards compatibility
//       with CAF 0.17. Remove with CAF 0.19.

#define CAF_ERROR_CODE_ENUM_1(type_name)                                       \
  namespace caf {                                                              \
  template <>                                                                  \
  struct is_error_code_enum<type_name> {                                       \
    static constexpr bool value = true;                                        \
  };                                                                           \
  }

#define CAF_ERROR_CODE_ENUM_2(type_name, unused)                               \
  CAF_ERROR_CODE_ENUM_1(type_name)

#ifdef CAF_MSVC
#  define CAF_ERROR_CODE_ENUM(...)                                             \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_ERROR_CODE_ENUM_,                           \
                               __VA_ARGS__)(__VA_ARGS__),                      \
               CAF_PP_EMPTY())
#else
#  define CAF_ERROR_CODE_ENUM(...)                                             \
    CAF_PP_OVERLOAD(CAF_ERROR_CODE_ENUM_, __VA_ARGS__)(__VA_ARGS__)
#endif
