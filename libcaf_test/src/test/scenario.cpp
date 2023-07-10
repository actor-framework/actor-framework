// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/scenario.hpp"

#include "caf/test/and_given.hpp"
#include "caf/test/and_when.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/given.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/when.hpp"

namespace caf::test {

block_type scenario::type() const noexcept {
  return block_type::scenario;
}

given* scenario::get_given(int id, std::string_view description,
                           const detail::source_location& loc) {
  return get_nested<given>(id, description, loc);
}

and_given* scenario::get_and_given(int id, std::string_view description,
                                   const detail::source_location& loc) {
  return get_nested<and_given>(id, description, loc);
}

when* scenario::get_when(int id, std::string_view description,
                         const detail::source_location& loc) {
  return get_nested<when>(id, description, loc);
}

and_when* scenario::get_and_when(int id, std::string_view description,
                                 const detail::source_location& loc) {
  return get_nested<and_when>(id, description, loc);
}

scope scenario::commit() {
  if (!ctx_->active() || !can_run())
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
