/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/sec.hpp"
#include "caf/atom.hpp"
#include "caf/actor.hpp"
#include "caf/config.hpp"
#include "caf/actor_cast.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace detail {

sync_request_bouncer::sync_request_bouncer(error r) : rsn(std::move(r)) {
  // nop
}

void sync_request_bouncer::operator()(const strong_actor_ptr& sender,
                                      const message_id& mid) const {
  if (sender && mid.is_request())
    sender->enqueue(nullptr, mid.response_id(),
                    make_message(make_error(sec::request_receiver_down)),
                    // TODO: this breaks out of the execution unit
                    nullptr);
}

void sync_request_bouncer::operator()(const mailbox_element& e) const {
  (*this)(e.sender, e.mid);
}

} // namespace detail
} // namespace caf
