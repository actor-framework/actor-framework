// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/section.hpp"

#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/scope.hpp"

namespace caf::test {

block_type section::type() const noexcept {
  return block_type::section;
}

section* section::get_section(int id, std::string_view description,
                              const detail::source_location& loc) {
  return get_nested<section>(id, description, loc);
}

scope section::commit() {
  if (!ctx_->active() || !can_run())
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
