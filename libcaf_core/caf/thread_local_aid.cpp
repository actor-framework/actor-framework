// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/thread_local_aid.hpp"

namespace caf {

namespace {

thread_local actor_id thread_local_aid_;

} // namespace

actor_id thread_local_aid() noexcept {
  return thread_local_aid_;
}

actor_id thread_local_aid(actor_id aid) noexcept {
  std::swap(aid, thread_local_aid_);
  return aid;
}

} // namespace caf
