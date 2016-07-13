/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include <poll.h>

#include <exception>

#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/io/detail/flare_actor.hpp"

namespace caf {
namespace io {
namespace detail {

flare_actor::flare_actor(actor_config& sys)
    : blocking_actor{sys},
      await_flare_{true} {
  // Ensure that the first enqueue operation returns unblocked_reader.
  mailbox().try_block();
}

void flare_actor::launch(execution_unit*, bool, bool) {
  // Nothing todo here since we only extract messages via receive() calls.
}

void flare_actor::act() {
  // Usually called from launch(). But should never happen in our
  // implementation.
  CAF_ASSERT(! "act() of flare_actor called");
}

void flare_actor::await_data() {
  CAF_LOG_DEBUG("awaiting data");
  if (! await_flare_)
    return;
  pollfd p = {flare_.fd(), POLLIN, 0};
  for (;;) {
    CAF_LOG_DEBUG("polling");
    auto n = ::poll(&p, 1, -1);
    if (n < 0 && errno != EAGAIN)
      std::terminate();
    if (n == 1) {
      CAF_ASSERT(p.revents & POLLIN);
      CAF_ASSERT(has_next_message());
      await_flare_ = false;
      return;
    }
  }
}

bool flare_actor::await_data(timeout_type timeout) {
  CAF_LOG_DEBUG("awaiting data with timeout");
  if (! await_flare_)
    return true;
  auto delta = timeout - timeout_type::clock::now();
  if (delta.count() <= 0)
    return false;
  pollfd p = {flare_.fd(), POLLIN, 0};
  auto n = ::poll(&p, 1, delta.count());
  if (n < 0 && errno != EAGAIN)
    std::terminate();
  if (n == 1) {
    CAF_ASSERT(p.revents & POLLIN);
    CAF_ASSERT(has_next_message());
    await_flare_ = false;
    return true;
  }
  return false;
}

void flare_actor::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  switch (mailbox().enqueue(ptr.release())) {
    case caf::detail::enqueue_result::unblocked_reader: {
      CAF_LOG_DEBUG("firing flare");
      flare_.fire();
      break;
    }
    case caf::detail::enqueue_result::queue_closed:
      if (mid.is_request()) {
        caf::detail::sync_request_bouncer bouncer{exit_reason()};
        bouncer(sender, mid);
      }
      break;
    case caf::detail::enqueue_result::success:
      break;
  }
}

mailbox_element_ptr flare_actor::dequeue() {
  auto msg = next_message();
  if (!has_next_message() && mailbox().try_block()) {
    auto extinguished = flare_.extinguish_one();
    CAF_ASSERT(extinguished);
    await_flare_ = true;
  }
  return msg;
}

const char* flare_actor::name() const {
  return "flare_actor";
}

int flare_actor::descriptor() const {
  return flare_.fd();
}

} // namespace detail
} // namespace io
} // namespace caf

