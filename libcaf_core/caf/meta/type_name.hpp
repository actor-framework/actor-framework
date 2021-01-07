// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/meta/annotation.hpp"

namespace caf::meta {

struct type_name_t : annotation {
  constexpr type_name_t(const char* cstr) : value(cstr) {
    // nop
  }

  const char* value;
};

/// Returns a type name annotation.
type_name_t constexpr type_name(const char* cstr) {
  return {cstr};
}
} // namespace caf::meta
