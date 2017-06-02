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

#include "caf/policy/pull5.hpp"

#include <numeric>

#include "caf/logger.hpp"
#include "caf/upstream_path.hpp"

namespace caf {
namespace policy {

pull5::~pull5() {
  // nop
}

void pull5::fill_assignment_vec(long downstream_credit) {
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
    auto y = std::min(5l, x + available);
    auto delta = y - x;
    if (delta >= min_credit_assignment()) {
      p.second = delta;
      available -= delta;
    } else {
      p.second = 0;
    }
  }
}

} // namespace policy
} // namespace caf
