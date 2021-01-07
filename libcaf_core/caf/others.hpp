// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <functional>
#include <type_traits>

#include "caf/catch_all.hpp"

namespace caf {

struct others_t {
  constexpr others_t() {
    // nop
  }

  template <class F>
  catch_all<F> operator>>(F fun) const {
    return {fun};
  }
};

constexpr others_t others = others_t{};

} // namespace caf
