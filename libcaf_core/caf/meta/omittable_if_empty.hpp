// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/meta/annotation.hpp"

namespace caf::meta {

struct omittable_if_empty_t : annotation {
  constexpr omittable_if_empty_t() {
    // nop
  }
};

/// Allows an inspector to omit the following data field if it is empty.
constexpr omittable_if_empty_t omittable_if_empty() {
  return {};
}

} // namespace caf::meta
