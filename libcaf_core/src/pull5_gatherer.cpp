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

#include "caf/detail/pull5_gatherer.hpp"

namespace caf {
namespace detail {

pull5_gatherer::pull5_gatherer(local_actor* selfptr) : super(selfptr) {
  // nop
}

void pull5_gatherer::assign_credit(long available) {
  CAF_LOG_TRACE(CAF_ARG(available));
  for (auto& kvp : assignment_vec_) {
    auto x = std::min(available, 5l - kvp.first->assigned_credit);
    available -= x;
    kvp.second = x;
  }
  emit_credits();
}

long pull5_gatherer::initial_credit(long, inbound_path*) {
  return 5;
}

} // namespace detail
} // namespace caf
