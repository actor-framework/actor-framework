// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive/atomic_control_block.hpp"

namespace caf::intrusive {

atomic_control_block::~atomic_control_block() noexcept {
  // nop
}

void atomic_control_block::deref() noexcept {
  ref_count_.dec_strong(this);
}

} // namespace caf::intrusive
