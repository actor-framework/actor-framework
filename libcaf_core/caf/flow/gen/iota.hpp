// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::flow::gen {

/// A generator that emits ascending values.
template <class T>
class iota {
public:
  using output_type = T;

  explicit iota(T init) : value_(std::move(init)) {
    // nop
  }

  iota(iota&&) = default;
  iota(const iota&) = default;
  iota& operator=(iota&&) = default;
  iota& operator=(const iota&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i)
      if (!step.on_next(value_++, steps...))
        return;
  }

private:
  T value_;
};

} // namespace caf::flow::gen
