// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/block.hpp"

#include "caf/test/context.hpp"
#include "caf/test/nesting_error.hpp"

namespace caf::test {

block::block(context_ptr ctx, int id, std::string_view description,
             const detail::source_location& loc)
  : ctx_(ctx), id_(id), description_(description), loc_(loc) {
  // nop
}

block::~block() {
  // nop
}

void block::enter() {
  executed_ = true;
  ctx_->on_enter(this);
}

void block::leave() {
  ctx_->on_leave(this);
}

void block::on_leave() {
  // nop
}

bool block::can_run() const noexcept {
  auto pred = [](const auto* nested) { return nested->can_run(); };
  return !executed_ || std::any_of(nested_.begin(), nested_.end(), pred);
}

section* block::get_section(int, std::string_view,
                            const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::section, loc);
}

given* block::get_given(int, std::string_view,
                        const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::given, loc);
}

and_given* block::get_and_given(int, std::string_view,
                                const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::given, loc);
}

when* block::get_when(int, std::string_view,
                      const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::when, loc);
}

and_when* block::get_and_when(int, std::string_view,
                              const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::and_when, loc);
}

then* block::get_then(int, std::string_view,
                      const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::then, loc);
}

and_then* block::get_and_then(int, std::string_view,
                              const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::and_then, loc);
}

but* block::get_but(int, std::string_view, const detail::source_location& loc) {
  nesting_error::raise_not_allowed(type(), block_type::but, loc);
}

std::unique_ptr<block>& block::get_nested_or_construct(int id) {
  // Note: we only need this trivial getter to have avoid including
  // "context.hpp" from "block.hpp".
  return ctx_->steps[id];
}

} // namespace caf::test
