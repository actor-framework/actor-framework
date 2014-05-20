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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include "cppa/atom.hpp"
#include "cppa/config.hpp"
#include "cppa/message_id.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/system_messages.hpp"

#include "cppa/detail/raw_access.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

namespace cppa {
namespace detail {

sync_request_bouncer::sync_request_bouncer(std::uint32_t r)
: rsn(r == exit_reason::not_exited ? exit_reason::normal : r) { }

void sync_request_bouncer::operator()(const actor_addr& sender,
                                      const message_id& mid) const {
    CPPA_REQUIRE(rsn != exit_reason::not_exited);
    if (sender && mid.is_request()) {
        auto ptr = detail::raw_access::get(sender);
        ptr->enqueue({invalid_actor_addr, ptr, mid.response_id()},
                     make_any_tuple(sync_exited_msg{sender, rsn}),
                     // TODO: this breaks out of the execution unit
                     nullptr);
    }
}

void sync_request_bouncer::operator()(const mailbox_element& e) const {
    (*this)(e.sender, e.mid);
}

} // namespace util
} // namespace cppa

