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

#include "caf/detail/call_cfun.hpp"

namespace caf {
namespace detail {

bool cc_zero(int value) {
  return value == 0;
}

bool cc_one(int value) {
  return value == 1;
}

bool cc_not_minus1(int value) {
  return value != -1;
}

bool cc_valid_socket(caf::io::network::native_socket fd) {
  return fd != caf::io::network::invalid_native_socket;
}

} // namespace detail
} // namespace caf
