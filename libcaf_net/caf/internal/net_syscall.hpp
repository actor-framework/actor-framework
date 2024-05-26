// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/format_to_error.hpp"
#include "caf/sec.hpp"

#include <cstdio>
#include <cstdlib>

/// Calls a C functions and returns an error if `var op rhs` returns `true`.
#define CAF_NET_SYSCALL(funname, var, op, rhs, expr)                           \
  auto var = expr;                                                             \
  if (var op rhs)                                                              \
  return format_to_error(sec::network_syscall_failed,                          \
                         "error in function {}: {}", funname,                  \
                         last_socket_error_as_string())

/// Calls a C functions and calls exit() if `var op rhs` returns `true`.
#define CAF_NET_CRITICAL_SYSCALL(funname, var, op, rhs, expr)                  \
  auto var = expr;                                                             \
  if (var op rhs) {                                                            \
    fprintf(stderr, "[FATAL] %s:%u: syscall failed: %s returned %s\n",         \
            __FILE__, __LINE__, funname,                                       \
            last_socket_error_as_string().c_str());                            \
    abort();                                                                   \
  }                                                                            \
  static_cast<void>(0)
