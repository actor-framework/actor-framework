/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_IO_NETWORK_OPERATION_HPP
#define CAF_IO_NETWORK_OPERATION_HPP

namespace caf {
namespace io {
namespace network {

/// Identifies network IO operations, i.e., read or write.
enum class operation {
  read,
  write,
  propagate_error
};

constexpr const char* to_string(operation op) {
  return op == operation::read ? "read"
                               : (op == operation::write ? "write"
                                                         : "propagate_error");
}

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_OPERATION_HPP
