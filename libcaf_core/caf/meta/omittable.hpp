// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/meta/annotation.hpp"

namespace caf::meta {

struct omittable_t : annotation {
  constexpr omittable_t() {
    // nop
  }
};

/// Allows an inspector to omit the following data field
/// unconditionally when producing human-friendly output.
constexpr omittable_t omittable() {
  return {};
}

} // namespace caf::meta
