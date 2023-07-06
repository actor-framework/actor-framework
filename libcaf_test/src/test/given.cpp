// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/given.hpp"

#include "caf/test/and_when.hpp"
#include "caf/test/context.hpp"
#include "caf/test/nesting_error.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/when.hpp"

namespace caf::test {

block_type given::type() const noexcept {
  return block_type::given;
}

when* given::get_when(int id, std::string_view description,
                      const detail::source_location& loc) {
  return get_nested<when>(id, description, loc);
}

and_when* given::get_and_when(int id, std::string_view description,
                              const detail::source_location& loc) {
  auto* result = ctx_->get<and_when>(id, description, loc);
  if (nested_.empty()) {
    nesting_error::raise_invalid_sequence(block_type::when,
                                          block_type::and_when, loc);
  }
  nested_.push_back(result);
  return result;
}

scope given::commit() {
  if (!ctx_->active() || !can_run())
    return {};
  enter();
  return scope{this};
}

  } // namespace caf::test
