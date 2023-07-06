// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/context.hpp"

#include "caf/test/block.hpp"
#include "caf/test/reporter.hpp"

namespace caf::test {

void context::on_enter(block* ptr) {
  call_stack.push_back(ptr);
  unwind_stack.clear();
  path.push_back(ptr);
  reporter::instance->begin_step(ptr);
}

void context::on_leave(block* ptr) {
  call_stack.pop_back();
  unwind_stack.push_back(ptr);
  reporter::instance->end_step(ptr);
}

bool context::can_run() {
  auto pred = [](auto& kvp) { return kvp.second->can_run(); };
  return !steps.empty() && std::any_of(steps.begin(), steps.end(), pred);
}

block* context::find_predecessor_block(int caller_id, block_type type) {
  // Find the caller.
  auto i = steps.find(caller_id);
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
