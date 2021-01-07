// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/mailbox_element.hpp"

namespace caf {

/// Convenience tag type for producing empty forwarding stacks.
struct no_stages_t {
  constexpr no_stages_t() {
    // nop
  }

  operator mailbox_element::forwarding_stack() const {
    return {};
  }
};

/// Convenience tag for producing empty forwarding stacks.
constexpr no_stages_t no_stages = no_stages_t{};

} // namespace caf
