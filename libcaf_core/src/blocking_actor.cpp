/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/exception.hpp"
#include "caf/blocking_actor.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {

blocking_actor::blocking_actor() {
  is_blocking(true);
}

blocking_actor::~blocking_actor() {
  // avoid weak-vtables warning
}

void blocking_actor::await_all_other_actors_done() {
  detail::singletons::get_actor_registry()->await_running_count_equal(1);
}

void blocking_actor::act() {
  CAF_LOG_TRACE("");
  if (initial_behavior_fac_)
    initial_behavior_fac_(this);
}

void blocking_actor::initialize() {
  // nop
}

void blocking_actor::dequeue(behavior& bhvr, message_id mid) {
  CAF_LOG_TRACE(CAF_MARG(mid, integer_value));
  // try to dequeue from cache first
  if (invoke_from_cache(bhvr, mid)) {
    return;
  }
  // requesting an invalid timeout will reset our active timeout
  uint32_t timeout_id = 0;
  if (mid == invalid_message_id) {
    timeout_id = request_timeout(bhvr.timeout());
  } else {
    request_sync_timeout_msg(bhvr.timeout(), mid);
  }
  // read incoming messages
  for (;;) {
    await_data();
    auto msg = next_message();
    switch (invoke_message(msg, bhvr, mid)) {
      case im_success:
        if (mid == invalid_message_id) {
          reset_timeout(timeout_id);
        }
        return;
      case im_skipped:
        if (msg) {
          push_to_cache(std::move(msg));
        }
        break;
      default:
        // delete msg
        break;
    }
  }
}

} // namespace caf
