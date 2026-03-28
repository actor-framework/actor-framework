// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/abstract_ref_counted.hpp"

namespace caf {

abstract_ref_counted::~abstract_ref_counted() noexcept {
  // nop
}

} // namespace caf
