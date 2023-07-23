// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class take_last {
public:
  using input_type = T;

  using output_type = T;

  explicit take_last(size_t num) : last_elements_count_(num) {
    // nop
  }

  take_last(take_last&&) = default;
  take_last(const take_last&) = default;
  take_last& operator=(take_last&&) = default;
  take_last& operator=(const take_last&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    elements_.emplace_back(std::move(item));
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    auto take_last_begin = elements_.begin();
    if (elements_.size() > last_elements_count_)
      take_last_begin += (elements_.size() - last_elements_count_);
    std::for_each(take_last_begin, elements_.end(),
                  [&](auto item) { next.on_next(item, steps...); });
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  size_t last_elements_count_;
  std::vector<output_type> elements_;
};

} // namespace caf::flow::step
