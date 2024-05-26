// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/native_socket.hpp"

#include "caf/error.hpp"
#include "caf/format_to_error.hpp"
#include "caf/sec.hpp"

#include <cstdio>
#include <cstdlib>

namespace caf::detail {

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
  return format_to_error(sec::network_syscall_failed, "{}: {}", fun_name,      \
                         last_socket_error_as_string())

/// Calls a C functions and calls exit() if `predicate(var)` returns false.
#define CALL_CRITICAL_CFUN(var, predicate, funname, expr)                      \
  auto var = expr;                                                             \
  if (!predicate(var)) {                                                       \
    fprintf(stderr, "[FATAL] %s:%u: syscall failed: %s returned %s\n",         \
            __FILE__, __LINE__, funname,                                       \
            last_socket_error_as_string().c_str());                            \
    abort();                                                                   \
  }                                                                            \
  static_cast<void>(0)

} // namespace caf::detail
