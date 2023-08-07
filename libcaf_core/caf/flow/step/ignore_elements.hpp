// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/ring_buffer.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class ignore_elements {
public:
  using input_type = T;

  using output_type = T;

  explicit ignore_elements() {
    // nop
  }

  ignore_elements(ignore_elements&&) = default;
  ignore_elements(const ignore_elements&) = default;
  ignore_elements& operator=(ignore_elements&&) = default;
  ignore_elements& operator=(const ignore_elements&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
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
