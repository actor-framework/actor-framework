// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/test.hpp"

#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/section.hpp"

namespace caf::test {

block_type test::type() const noexcept {
  return block_type::test;
}

section* test::get_section(int id, std::string_view description,
                           const detail::source_location& loc) {
  return get_nested<section>(id, description, loc);
}

scope test::commit() {
  if (!ctx_->active() || !can_run())
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
