// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/meta/annotation.hpp"

namespace caf::meta {

struct omittable_if_none_t : annotation {
  constexpr omittable_if_none_t() {
    // nop
  }
};

[[deprecated]] constexpr omittable_if_none_t omittable_if_none() {
  return {};
}

} // namespace caf::meta
