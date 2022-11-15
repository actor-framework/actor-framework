// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <cstddef>

namespace caf::flow::step {

template <class T>
class take {
public:
  using input_type = T;

  using output_type = T;

  explicit take(size_t num) : remaining_(num) {
    // nop
  }

  take(take&&) = default;
  take(const take&) = default;
  take& operator=(take&&) = default;
  take& operator=(const take&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (remaining_ > 0) {
      if (next.on_next(item, steps...)) {
        if (--remaining_ > 0) {
          return true;
        } else {
          next.on_complete(steps...);
          return false;
        }
      }
    }
    return false;
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
