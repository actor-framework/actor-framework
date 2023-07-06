// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/then.hpp"

#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/scope.hpp"

namespace caf::test {

block_type then::type() const noexcept {
  return block_type::then;
}

scope then::commit() {
  if (!ctx_->active() || executed_)
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
