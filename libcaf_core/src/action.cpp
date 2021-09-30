// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/action.hpp"

#include "caf/logger.hpp"

namespace caf {

action::transition action::run() {
  CAF_LOG_TRACE("");
  CAF_ASSERT(pimpl_ != nullptr);
  return pimpl_->run();
}

} // namespace caf
