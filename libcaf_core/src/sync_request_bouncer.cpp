/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/actor_cast.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace detail {

sync_request_bouncer::sync_request_bouncer(uint32_t r)
        : rsn(r == exit_reason::not_exited ? exit_reason::normal : r) {}

void sync_request_bouncer::operator()(const actor_addr& sender,
                                      const message_id& mid) const {
    CAF_REQUIRE(rsn != exit_reason::not_exited);
    if (sender && mid.is_request()) {
        auto ptr = actor_cast<abstract_actor_ptr>(sender);
        ptr->enqueue(invalid_actor_addr, mid.response_id(),
                     make_message(sync_exited_msg{sender, rsn}),
                     // TODO: this breaks out of the execution unit
                     nullptr);
    }
}

void sync_request_bouncer::operator()(const mailbox_element& e) const {
    (*this)(e.sender, e.mid);
}

} // namespace detail
} // namespace caf
