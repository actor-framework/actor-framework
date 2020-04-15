/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/config.hpp"

#ifdef CAF_ENABLE_EXCEPTIONS
#  include <stdexcept>
#endif

#include "caf/detail/core_export.hpp"
#include "caf/detail/pp.hpp"

namespace caf::detail {

CAF_CORE_EXPORT void log_cstring_error(const char* cstring);

} // namespace caf::detail

#ifdef CAF_ENABLE_EXCEPTIONS

#  define CAF_RAISE_ERROR_IMPL_2(exception_type, msg)                          \
    do {                                                                       \
      ::caf::detail::log_cstring_error(msg);                                   \
      throw exception_type(msg);                                               \
    } while (false)

#  define CAF_RAISE_ERROR_IMPL_1(msg)                                          \
    CAF_RAISE_ERROR_IMPL_2(std::runtime_error, msg)

#else // CAF_ENABLE_EXCEPTIONS

#  define CAF_RAISE_ERROR_IMPL_1(msg)                                          \
    do {                                                                       \
      ::caf::detail::log_cstring_error(msg);                                   \
      CAF_CRITICAL(msg);                                                       \
    } while (false)

#  define CAF_RAISE_ERROR_IMPL_2(unused, msg) CAF_RAISE_ERROR_IMPL_1(msg)

#endif // CAF_ENABLE_EXCEPTIONS

#ifdef CAF_MSVC

/// Throws an exception if `CAF_ENABLE_EXCEPTIONS` is defined, otherwise calls
/// abort() after printing a given message.
#  define CAF_RAISE_ERROR(...)                                                 \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_RAISE_ERROR_IMPL_,                          \
                               __VA_ARGS__)(__VA_ARGS__),                      \
               CAF_PP_EMPTY())

#else // CAF_MSVC

/// Throws an exception if `CAF_ENABLE_EXCEPTIONS` is defined, otherwise calls
/// abort() after printing a given message.
#  define CAF_RAISE_ERROR(...)                                                 \
    CAF_PP_OVERLOAD(CAF_RAISE_ERROR_IMPL_, __VA_ARGS__)(__VA_ARGS__)

#endif // CAF_MSVC
