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

#include <cstdio>
#include <cstdlib>

#include "caf/error.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/sec.hpp"

namespace caf {
namespace detail {

/// Predicate for `ccall` meaning "expected result of f is 0".
inline bool cc_zero(int value) {
  return value == 0;
}

/// Predicate for `ccall` meaning "expected result of f is 1".
inline bool cc_one(int value) {
  return value == 1;
}

/// Predicate for `ccall` meaning "expected result of f is not -1".
inline bool cc_not_minus1(int value) {
  return value != -1;
}

/// Predicate for `ccall` meaning "expected result of f is a valid socket".
inline bool cc_valid_socket(caf::io::network::native_socket fd) {
  return fd != caf::io::network::invalid_native_socket;
}

/// Calls a C functions and returns an error if `predicate(var)` returns false.
#define CALL_CFUN(var, predicate, fun_name, expr)                              \
  auto var = expr;                                                             \
  if (!predicate(var))                                                         \
    return make_error(sec::network_syscall_failed,                             \
                      fun_name, last_socket_error_as_string())

/// Calls a C functions and calls exit() if `predicate(var)` returns false.
#define CALL_CRITICAL_CFUN(var, predicate, funname, expr)                      \
  auto var = expr;                                                             \
  if (!predicate(var)) {                                                       \
    fprintf(stderr, "[FATAL] %s:%u: syscall failed: %s returned %s\n",         \
           __FILE__, __LINE__, funname, last_socket_error_as_string().c_str());\
    abort();                                                                   \
  } static_cast<void>(0)

} // namespace detail
} // namespace caf
