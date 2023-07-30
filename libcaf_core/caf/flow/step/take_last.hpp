// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/ring_buffer.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class take_last {
public:
  using input_type = T;

  using output_type = T;

  explicit take_last(size_t num)
    : last_elements_count_(num),
      elements_(caf::detail::ring_buffer<output_type>(num)) {
    // nop
  }

  take_last(take_last&&) = default;
  take_last(const take_last&) = default;
  take_last& operator=(take_last&&) = default;
  take_last& operator=(const take_last&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    elements_.push_back(item);
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    while (!elements_.empty()) {
      if (!next.on_next(elements_.front(), steps...))
        break;
      elements_.pop_front();
    };
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  size_t last_elements_count_;
  caf::detail::ring_buffer<output_type> elements_;
};

} // namespace caf::flow::step
