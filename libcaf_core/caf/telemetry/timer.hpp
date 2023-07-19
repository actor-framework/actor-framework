// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/telemetry/histogram.hpp"

#include <chrono>

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
