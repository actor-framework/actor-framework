// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"

#include <utility>

namespace caf::flow::step {

template <class T>
class on_error_return_item {
public:
  using input_type = T;

  using output_type = T;

  explicit on_error_return_item(T&& item) : item_(std::forward<T>(item)) {
    // nop
  }

  template <class Next, class... Steps>
  bool on_next(const T& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    if (next.on_next(item_, steps...))
      next.on_complete(steps...);
    else
      next.on_error(what, steps...);
  }

private:
  input_type item_;
};

} // namespace caf::flow::step
