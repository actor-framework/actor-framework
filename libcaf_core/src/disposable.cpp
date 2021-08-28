// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/disposable.hpp"

namespace caf {

disposable::impl::~impl() {
  // nop
}

disposable disposable::impl::as_disposable() noexcept {
  return disposable{intrusive_ptr<disposable::impl>{this}};
}

} // namespace caf
