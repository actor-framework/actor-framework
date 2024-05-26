// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf::flow::gen {

/// A generator that emits nothing and calls `on_complete` immediately.
template <class T>
class empty {
public:
  using output_type = T;

  template <class Step, class... Steps>
  void pull(size_t, Step& step, Steps&... steps) {
    step.on_complete(steps...);
  }
};

} // namespace caf::flow::gen
