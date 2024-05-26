// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/skip.hpp"

#include "caf/message.hpp"
#include "caf/result.hpp"

namespace caf {

namespace {

skippable_result skip_fun_impl(scheduled_actor*, message&) {
  return skip;
}

} // namespace

skip_t::operator fun() const {
  return skip_fun_impl;
}

} // namespace caf
