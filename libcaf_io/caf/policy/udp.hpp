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

/// Function signature of read_datagram
using read_datagram_fun = decltype(io::network::read_datagram)*;

/// Function signature of write_datagram
using write_datagram_fun = decltype(io::network::write_datagram)*;

/// Policy object for wrapping default UDP operations
struct udp {
  static read_datagram_fun read_datagram;
  static write_datagram_fun write_datagram;
};

} // namespace policy
} // namespace caf
