/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/policy/anycast.hpp"

#include "caf/abstract_downstream.hpp"

namespace caf {
namespace policy {

long anycast::total_net_credit(const abstract_downstream& out) {
  // The total amount of available net credit is calculated as:
  // `av + (n * mb) - bs`, where `av` is the sum of all available credit on all
  // paths, `n` is the number of downstream paths, `mb` is the minimum buffer
  // size, and `bs` is the current buffer size.
  return (out.total_credit() + (out.num_paths() * out.min_buffer_size()))
         - out.buf_size();
}

void anycast::push(abstract_downstream& out, long* hint) {
  out.anycast(hint);
}

} // namespace policy
} // namespace caf
