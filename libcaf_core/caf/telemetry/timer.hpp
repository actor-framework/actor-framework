/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include <chrono>

#include "caf/telemetry/histogram.hpp"

namespace caf::telemetry {

/// Convenience helper for measuring durations such as latency using a histogram
/// with second resolution. The measurement starts when creating the objects and
/// finishes when the timer goes out of scope.
class timer {
public:
  using clock_type = std::chrono::steady_clock;

  explicit timer(dbl_histogram* h) : h_(h) {
    if (h)
      start_ = clock_type::now();
  }

  ~timer() {
    if (h_)
      observe(h_, start_);
  }

  auto histogram_ptr() const noexcept {
    return h_;
  }

  auto started() const noexcept {
    return start_;
  }

  static void observe(dbl_histogram* h, clock_type::time_point start) {
    using dbl_sec = std::chrono::duration<double>;
    auto end = clock_type::now();
    h->observe(std::chrono::duration_cast<dbl_sec>(end - start).count());
  }

private:
  dbl_histogram* h_;
  clock_type::time_point start_;
};

} // namespace caf::telemetry
