// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::flow::gen {

/// A generator that emits the same value repeatedly.
template <class T>
class repeat {
public:
  using output_type = T;

  explicit repeat(T value) : value_(std::move(value)) {
    // nop
  }

  repeat(repeat&&) = default;
  repeat(const repeat&) = default;
  repeat& operator=(repeat&&) = default;
  repeat& operator=(const repeat&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i)
      if (!step.on_next(value_, steps...))
        return;
  }

private:
  T value_;
};

} // namespace caf::flow::gen
