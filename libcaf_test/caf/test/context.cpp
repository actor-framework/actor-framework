// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/context.hpp"

#include "caf/test/block.hpp"
#include "caf/test/reporter.hpp"

#include <algorithm>

namespace caf::test {

context::~context() {
  // Out-of-line for the `steps` member variable since the header only has a
  // forward declaration to `block`.
}

// -- properties ---------------------------------------------------------------

bool context::active() const noexcept {
  return unwind_stack.empty();
}

/// Checks whether this block has at least one branch that can be executed.
bool context::can_run() const noexcept {
  auto pred = [](auto& kvp) { return kvp.second->can_run(); };
  return std::any_of(steps.begin(), steps.end(), pred);
}

/// Checks whether `ptr` has been activated this run, i.e., whether we can
/// find it in `unwind_stack`.
bool context::activated(block* ptr) const noexcept {
  return std::find(path.begin(), path.end(), ptr) != path.end();
}

/// Tries to find `name` in `parameters` and otherwise raises an exception.
const std::string& context::parameter(const std::string& name) const {
  auto i = parameters.find(name);
  if (i == parameters.end()) {
    auto msg = "missing parameter: " + name;
    CAF_RAISE_ERROR(std::runtime_error, msg.c_str());
  }
  return i->second;
}

// -- mutators -----------------------------------------------------------------

void context::clear_stacks() {
  call_stack.clear();
  unwind_stack.clear();
  path.clear();
}

void context::on_enter(block* ptr) {
  call_stack.push_back(ptr);
  unwind_stack.clear();
  path.push_back(ptr);
  reporter::instance().begin_step(ptr);
}

void context::on_leave(block* ptr) {
  call_stack.pop_back();
  unwind_stack.push_back(ptr);
  reporter::instance().end_step(ptr);
}

block* context::find_predecessor_block(int caller_id, block_type type) {
  // Find the caller.
  auto i = steps.find(std::make_pair(caller_id, example_id));
  if (i == steps.end())
    return nullptr;
  // Find the first step of type `T` that precedes the caller.
  auto pred = [type](auto& kvp) { return kvp.second->type() == type; };
  auto j = std::find_if(std::reverse_iterator{i}, steps.rend(), pred);
  if (j == steps.rend())
    return nullptr;
  return j->second.get();
}

} // namespace caf::test
