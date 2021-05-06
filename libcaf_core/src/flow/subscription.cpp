// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/subscription.hpp"

namespace caf::flow {

subscription::impl::~impl() {
  // nop
}

void subscription::impl::dispose() {
  cancel();
}

} // namespace caf::flow
