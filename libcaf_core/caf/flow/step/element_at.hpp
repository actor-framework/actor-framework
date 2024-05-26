// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class element_at {
public:
  using input_type = T;

  using output_type = T;

  explicit element_at(size_t index) : element_index_(index) {
    // nop
  }

  element_at(element_at&&) = default;
  element_at(const element_at&) = default;
  element_at& operator=(element_at&&) = default;
  element_at& operator=(const element_at&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (element_index_ == current_index_) {
      if (next.on_next(item, steps...))
        on_complete(next, steps...);
      return false;
    }
    ++current_index_;
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

private:
  size_t element_index_;
  size_t current_index_ = 0;
};

} // namespace caf::flow::step
