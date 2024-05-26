// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

#ifdef CAF_ENABLE_EXCEPTIONS
#  include <stdexcept>
#endif

#include "caf/detail/core_export.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/pp.hpp"

#include <type_traits>

namespace caf::detail {

CAF_CORE_EXPORT void log_cstring_error(const char* cstring);

#ifdef CAF_ENABLE_EXCEPTIONS

template <class T>
[[noreturn]] std::enable_if_t<std::is_constructible<T, const char*>::value>
throw_impl(const char* msg) {
  throw T{msg};
}

template <class T>
[[noreturn]] void throw_impl(...) {
  throw T{};
}

#endif // CAF_ENABLE_EXCEPTIONS

} // namespace caf::detail

#ifdef CAF_ENABLE_EXCEPTIONS

#  define CAF_RAISE_ERROR_IMPL_2(exception_type, msg)                          \
    do {                                                                       \
      ::caf::detail::log_cstring_error(msg);                                   \
      ::caf::detail::throw_impl<exception_type>(msg);                          \
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
