// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/ring_buffer.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class skip_last {
public:
  using input_type = T;

  using output_type = T;

  explicit skip_last(size_t num) : elements_(num) {
    // nop
  }

  skip_last(skip_last&&) = default;
  skip_last(const skip_last&) = default;
  skip_last& operator=(skip_last&&) = default;
  skip_last& operator=(const skip_last&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (elements_.full()) {
      if (!next.on_next(elements_.front(), steps...))
        return false;
      elements_.pop_front();
    }
    elements_.push_back(item);
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
  detail::ring_buffer<output_type> elements_;
};

} // namespace caf::flow::step
