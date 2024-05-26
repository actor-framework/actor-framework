// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/when.hpp"

#include "caf/test/and_then.hpp"
#include "caf/test/context.hpp"
#include "caf/test/nesting_error.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/then.hpp"

namespace caf::test {

then* when::get_then(int id, std::string_view description,
                     const detail::source_location& loc) {
  auto* result = ctx_->get<then>(id, description, loc);
  if (nested_.empty()) {
    nested_.emplace_back(result);
  } else if (nested_.front() != result) {
    nesting_error::raise_too_many(type(), block_type::then, loc);
  }
  return result;
}

and_then* when::get_and_then(int id, std::string_view description,
                             const detail::source_location& loc) {
  auto* result = ctx_->get<and_then>(id, description, loc);
  if (nested_.empty()) {
    nesting_error::raise_invalid_sequence(block_type::then,
                                          block_type::and_then, loc);
  }
  nested_.push_back(result);
  return result;
}

block_type when::type() const noexcept {
  return block_type::when;
}

scope when::commit() {
  if (!ctx_->active() || !can_run())
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
