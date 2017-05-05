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

#include "caf/policy/greedy.hpp"

#include <numeric>

#include "caf/logger.hpp"
#include "caf/upstream_path.hpp"

namespace caf {
namespace policy {

greedy::greedy() : low_watermark(0), high_watermark(5) {
  // nop
}

void greedy::assign_credit(assignment_vec& xs,
                           long total_downstream_net_credit) {
  CAF_LOG_TRACE(CAF_ARG(xs) << CAF_ARG(total_downstream_net_credit));
  // Zero-out assignment vector if no credit is available at downstream paths.
  if (total_downstream_net_credit <= 0) {
    for (auto& x : xs)
      x.second = 0;
    return;
  }
  // Assign credit to upstream paths until no more credit is available. We must
  // make sure to write to each element in the vector.
  auto available = total_downstream_net_credit;
  for (auto& p : xs) {
    auto& x = p.first->assigned_credit;
    if (x < high_watermark) {
      p.second = std::min(high_watermark - x, available);
      available -= p.second;
    } else {
      p.second = 0;
    }
  }
}

} // namespace policy
} // namespace caf
