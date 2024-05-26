// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/and_then.hpp"

#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/then.hpp"

namespace caf::test {

block_type and_then::type() const noexcept {
  return block_type::and_then;
}

scope and_then::commit() {
  // An AND_THEN block is only executed if the previous THEN block was executed.
  if (!can_run() || !ctx_->activated(ctx_->find_predecessor<then>(id_)))
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
