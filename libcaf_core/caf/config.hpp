/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_CONFIG_HPP
#define CAF_CONFIG_HPP

// Config pararameters defined by the build system (usually CMake):
//
// CAF_ENABLE_RUNTIME_CHECKS:
//   - check requirements at runtime
//
// CAF_LOG_LEVEL:
//   - denotes the amount of logging, ranging from error messages only (0)
//     to complete traces (4)

/**
 * Denotes the libcaf version in the format {MAJOR}{MINOR}{PATCH},
 * whereas each number is a two-digit decimal number without
 * leading zeros (e.g. 900 is version 0.9.0).
 */
#define CAF_VERSION 1102

#define CAF_MAJOR_VERSION (CAF_VERSION / 10000)
#define CAF_MINOR_VERSION ((CAF_VERSION / 100) % 100)
#define CAF_PATCH_VERSION (CAF_VERSION % 100)

// sets CAF_DEPRECATED, CAF_ANNOTATE_FALLTHROUGH,
// CAF_PUSH_WARNINGS and CAF_POP_WARNINGS
#if defined(__clang__)
#  define CAF_CLANG
#  define CAF_DEPRECATED __attribute__((__deprecated__))
#  define CAF_PUSH_WARNINGS                                                    \
    _Pragma("clang diagnostic push")                                           \
    _Pragma("clang diagnostic ignored \"-Wall\"")                              \
    _Pragma("clang diagnostic ignored \"-Wextra\"")                            \
    _Pragma("clang diagnostic ignored \"-Werror\"")                            \
    _Pragma("clang diagnostic ignored \"-Wdeprecated\"")                       \
    _Pragma("clang diagnostic ignored \"-Wdisabled-macro-expansion\"")         \
    _Pragma("clang diagnostic ignored \"-Wextra-semi\"")                       \
    _Pragma("clang diagnostic ignored \"-Wdocumentation\"")                    \
    _Pragma("clang diagnostic ignored \"-Wweak-vtables\"")                     \
    _Pragma("clang diagnostic ignored \"-Wunused-parameter\"")                 \
    _Pragma("clang diagnostic ignored \"-Wswitch-enum\"")                      \
    _Pragma("clang diagnostic ignored \"-Wshadow\"")                           \
    _Pragma("clang diagnostic ignored \"-Wconversion\"")                       \
    _Pragma("clang diagnostic ignored \"-Wcast-align\"")                       \
    _Pragma("clang diagnostic ignored \"-Wundef\"")                            \
    _Pragma("clang diagnostic ignored \"-Wnested-anon-types\"")
#  define CAF_POP_WARNINGS                          \
    _Pragma("clang diagnostic pop")
#  define CAF_ANNOTATE_FALLTHROUGH [[clang::fallthrough]]
#elif defined(__GNUC__)
#  define CAF_GCC
#  define CAF_DEPRECATED __attribute__((__deprecated__))
#  define CAF_PUSH_WARNINGS
#  define CAF_POP_WARNINGS
#  define CAF_ANNOTATE_FALLTHROUGH static_cast<void>(0)
#elif defined(_MSC_VER)
#  define CAF_MSVC
#  define CAF_DEPRECATED
#  define CAF_PUSH_WARNINGS
#  define CAF_POP_WARNINGS
#  define CAF_ANNOTATE_FALLTHROUGH static_cast<void>(0)
#else
#  define CAF_DEPRECATED
#  define CAF_PUSH_WARNINGS
#  define CAF_POP_WARNINGS
#  define CAF_ANNOTATE_FALLTHROUGH static_cast<void>(0)
#endif

// detect OS
#if defined(__APPLE__)
#  define CAF_MACOS
#  ifndef _GLIBCXX_HAS_GTHREADS
#  define _GLIBCXX_HAS_GTHREADS
#  endif
#elif defined(__linux__)
#  define CAF_LINUX
#   include <linux/version.h>
#   if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16)
#   define CAF_POLL_IMPL
#   endif
#elif defined(__FreeBSD__)
#  define CAF_BSD
#elif defined(WIN32) || defined(_WIN32)
#  define CAF_WINDOWS
#else
#  error Platform and/or compiler not supportet
#endif

#include <memory>
#include <cstdio>
#include <cstdlib>

// import backtrace and backtrace_symbols_fd into caf::detail
#ifdef CAF_WINDOWS
#include "caf/detail/execinfo_windows.hpp"
#else
#include <execinfo.h>
namespace caf {
namespace detail {
using ::backtrace;
using ::backtrace_symbols_fd;
} // namespace detail
} // namespace caf

#endif

#ifdef CAF_ENABLE_RUNTIME_CHECKS
#   define CAF_REQUIRE__(stmt, file, line)                                     \
    printf("%s:%u: requirement failed '%s'\n", file, line, stmt); {            \
      void* array[10];                                                         \
      auto caf_bt_size = ::caf::detail::backtrace(array, 10);                  \
      ::caf::detail::backtrace_symbols_fd(array, caf_bt_size, 2);              \
    } abort()
#   define CAF_REQUIRE(stmt)                                                   \
    if (static_cast<bool>(stmt) == false) {                                    \
      CAF_REQUIRE__(#stmt, __FILE__, __LINE__);                                \
    } static_cast<void>(0)
#else
#   define CAF_REQUIRE(unused) static_cast<void>(0)
#endif

#define CAF_CRITICAL__(error, file, line) {                                    \
    printf("%s:%u: critical error: '%s'\n", file, line, error);                \
    exit(7);                                                                   \
  } static_cast<void>(0)

#define CAF_CRITICAL(error) CAF_CRITICAL__(error, __FILE__, __LINE__)

#endif // CAF_CONFIG_HPP
