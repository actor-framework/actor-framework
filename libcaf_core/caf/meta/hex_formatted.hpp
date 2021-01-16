// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/meta/annotation.hpp"

namespace caf::meta {

struct hex_formatted_t : annotation {
  constexpr hex_formatted_t() {
    // nop
  }
};

[[deprecated]] constexpr hex_formatted_t hex_formatted() {
  return {};
}

} // namespace caf::meta
