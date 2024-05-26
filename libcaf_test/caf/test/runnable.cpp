// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/runnable.hpp"

#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/outline.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/test.hpp"

#include "caf/detail/scope_guard.hpp"

namespace caf::test {

namespace {

thread_local runnable* current_runnable;

} // namespace

runnable::~runnable() {
  // nop
}

void runnable::run() {
  current_runnable = this;
  auto guard
    = detail::scope_guard{[]() noexcept { current_runnable = nullptr; }};
  switch (root_type_) {
    case block_type::outline:
      if (auto guard = ctx_->get<outline>(0, description_, loc_)->commit()) {
        do_run();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select  the root block for the outline");
      break;
    case block_type::scenario:
      if (auto guard = ctx_->get<scenario>(0, description_, loc_)->commit()) {
        do_run();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select  the root block for the scenario");
      break;
    case block_type::test:
      if (auto guard = ctx_->get<test>(0, description_, loc_)->commit()) {
        do_run();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select the root block for the test");
      break;
    default:
      CAF_RAISE_ERROR(std::logic_error, "invalid root type");
  }
}

void runnable::call_do_run() {
  current_runnable = this;
  auto guard
    = detail::scope_guard{[]() noexcept { current_runnable = nullptr; }};
  do_run();
}

bool runnable::check(bool value, const detail::source_location& location) {
  if (value) {
    reporter::instance().pass(location);
  } else {
    reporter::instance().fail("should be true", location);
  }
  return value;
}

void runnable::require(bool value, const detail::source_location& location) {
  if (!check(value, location))
    requirement_failed::raise(location);
}

runnable& runnable::current() {
  auto ptr = current_runnable;
  if (!ptr)
    CAF_RAISE_ERROR(std::logic_error, "no current runnable");
  return *ptr;
}

block& runnable::current_block() {
  if (ctx_->call_stack.empty())
    CAF_RAISE_ERROR(std::logic_error, "no current block");
  return *ctx_->call_stack.back();
}

} // namespace caf::test
