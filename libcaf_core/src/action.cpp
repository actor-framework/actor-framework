// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/action.hpp"

#include "caf/logger.hpp"

namespace caf {

action::action(impl_ptr ptr) noexcept : pimpl_(std::move(ptr)) {
  // nop
}

} // namespace caf
