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
#include "caf/sec.hpp"

/// Calls a C functions and returns an error if `var op rhs` returns `true`.
#define CAF_NET_SYSCALL(funname, var, op, rhs, expr)                           \
  auto var = expr;                                                             \
  if (var op rhs)                                                              \
  return make_error(sec::network_syscall_failed, funname,                      \
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
