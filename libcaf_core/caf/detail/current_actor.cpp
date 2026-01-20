// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/current_actor.hpp"

namespace caf::detail {

namespace {

thread_local abstract_actor* current_actor_ptr;

} // namespace

abstract_actor* current_actor() noexcept {
  return current_actor_ptr;
}

void current_actor(abstract_actor* ptr) noexcept {
  current_actor_ptr = ptr;
}

current_actor_guard::current_actor_guard(abstract_actor* ptr) noexcept
  : prev_(current_actor_ptr) {
  current_actor_ptr = ptr;
}

current_actor_guard::~current_actor_guard() noexcept {
  current_actor_ptr = prev_;
}

} // namespace caf::detail
