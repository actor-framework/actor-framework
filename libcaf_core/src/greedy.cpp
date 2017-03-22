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

greedy::greedy() : low_watermark(0), high_watermark(5), min_buffer_size(5) {
  // nop
}

void greedy::assign_credit(assignment_vec& xs, size_t buf_size,
                           size_t downstream_credit) {
  CAF_LOG_TRACE(CAF_ARG(xs) << CAF_ARG(buf_size) << CAF_ARG(downstream_credit));
  /// Calculate how much credit we can hand out and how much credit we have
  /// already assigned. Buffered elements are counted as assigned credit,
  /// because we "release" credit only after pushing elements downstram.
  size_t max_available = downstream_credit + min_buffer_size;
  size_t assigned =
    buf_size + std::accumulate(xs.begin(), xs.end(), size_t{0},
                               [](size_t x, const assignment_pair& y) {
                                 return x + y.first->assigned_credit;
                               });
  if (assigned >= max_available) {
    // zero out assignment vector
    for (auto& x : xs)
      x.second = 0;
    return;
  }
  // Assign credit to upstream paths until no more credit is available. We must
  // make sure to write to each element in the vector.
  auto available = max_available - assigned;
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
