// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::flow::gen {

/// A generator that emits a single value once.
template <class T>
class just {
public:
  using output_type = T;

  explicit just(T value) : value_(std::move(value)) {
    // nop
  }

  just(just&&) = default;
  just(const just&) = default;
  just& operator=(just&&) = default;
  just& operator=(const just&) = default;

  template <class Step, class... Steps>
  void pull([[maybe_unused]] size_t n, Step& step, Steps&... steps) {
    if (step.on_next(value_, steps...))
      step.on_complete(steps...);
  }

private:
  T value_;
};

} // namespace caf::flow::gen
