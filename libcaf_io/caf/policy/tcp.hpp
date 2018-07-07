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

#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace policy {

/// Function signature of `read_some`.
using read_some_fun = decltype(io::network::read_some)*;

/// Function signature of `wite_some`.
using write_some_fun = decltype(io::network::write_some)*;

/// Function signature of `try_accept`.
using try_accept_fun = decltype(io::network::try_accept)*;

/// Policy object for wrapping default TCP operations.
struct tcp {
  static read_some_fun read_some;
  static write_some_fun write_some;
  static try_accept_fun try_accept;
};

} // namespace policy
} // namespace caf
