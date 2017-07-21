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

#include "caf/random_gatherer.hpp"

namespace caf {

random_gatherer::random_gatherer(local_actor* selfptr) : super(selfptr) {
  // nop
}

random_gatherer::~random_gatherer() {
  // nop
}

void random_gatherer::assign_credit(long available) {
    CAF_LOG_TRACE(CAF_ARG(available));
    for (auto& kvp : assignment_vec_) {
      auto x = std::min(available, max_credit() - kvp.first->assigned_credit);
      available -= x;
      kvp.second = x;
    }
    emit_credits();
}

long random_gatherer::initial_credit(long available, path_type*) {
  return std::min(available, max_credit());
}

/*
void random_gatherer::fill_assignment_vec(long downstream_credit) {
  CAF_LOG_TRACE(CAF_ARG(downstream_credit));
  // Zero-out assignment vector if no credit is available at downstream paths.
  if (downstream_credit <= 0) {
    for (auto& x : assignment_vec_)
      x.second = 0;
    return;
  }
  // Assign credit to upstream paths until no more credit is available. We must
  // make sure to write to each element in the vector.
  auto available = downstream_credit;
  for (auto& p : assignment_vec_) {
    auto& x = p.first->assigned_credit; // current value
    auto y = std::min(max_credit(), x + available);
    auto delta = y - x;
    if (delta >= min_credit_assignment()) {
      p.second = delta;
      available -= delta;
    } else {
      p.second = 0;
    }
  }
}
*/

} // namespace caf
