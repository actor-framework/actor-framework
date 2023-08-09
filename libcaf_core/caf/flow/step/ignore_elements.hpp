// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

namespace caf::flow::step {

/// A step that ignores all elements and only forwards calls to `on_complete`
/// and `on_error`.
template <class T>
class ignore_elements {
public:
  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type&, Next&, Steps&...) {
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

} // namespace caf::flow::step
