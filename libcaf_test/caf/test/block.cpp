// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/block.hpp"

#include "caf/test/context.hpp"
#include "caf/test/nesting_error.hpp"

#include <algorithm>

namespace caf::test {

block::block(context* ctx, int id, std::string_view description,
             const detail::source_location& loc)
  : ctx_(ctx), id_(id), raw_description_(description), loc_(loc) {
  // nop
}

block::~block() {
  // nop
}

void block::enter() {
  lazy_init();
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
  return ctx_->steps[std::make_pair(id, ctx_->example_id)];
}

namespace {

enum class cpy_state {
  verbatim,
  start_name,
  read_name,
};

} // namespace

void block::lazy_init() {
  // Note: we need to delay the initialization of the description until the
  //       block is actually executed, because the context might not be fully
  //       initialized when the block is constructed.
  if (!description_.empty() || raw_description_.empty())
    return;
  description_.reserve(raw_description_.size());
  // Process <arg> syntax if the root block is an outline.
  if (!ctx_->call_stack.empty()
      && ctx_->call_stack.front()->type() == block_type::outline) {
    std::string parameter_name;
    auto state = cpy_state::verbatim;
    for (auto c : raw_description_) {
      switch (state) {
        default: // cpy_state::verbatim:
          switch (c) {
            case '<':
              state = cpy_state::start_name;
              break;
            default:
              description_ += c;
              break;
          }
          break;
        case cpy_state::start_name:
          switch (c) {
            case ' ':
            case '>':
              description_ += '<';
              description_ += c;
              state = cpy_state::verbatim;
              break;
            default:
              parameter_name.clear();
              parameter_name += c;
              state = cpy_state::read_name;
              break;
          }
          break;
        case cpy_state::read_name:
          switch (c) {
            case '>':
              description_ += ctx_->parameter(parameter_name);
              parameter_names_.push_back(std::move(parameter_name));
              parameter_name.clear();
              state = cpy_state::verbatim;
              break;
            default:
              parameter_name += c;
              break;
          }
          break;
      }
    }
  } else {
    description_ = raw_description_;
  }
}

} // namespace caf::test
