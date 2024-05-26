// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class skip {
public:
  using input_type = T;

  using output_type = T;

  explicit skip(size_t num) : remaining_(num) {
    // nop
  }

  skip(skip&&) = default;
  skip(const skip&) = default;
  skip& operator=(skip&&) = default;
  skip& operator=(const skip&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (remaining_ == 0) {
      return next.on_next(item, steps...);
    } else {
      --remaining_;
      return true;
    }
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
  size_t remaining_;
};

} // namespace caf::flow::step
