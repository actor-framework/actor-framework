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


#ifndef CPPA_SYNC_REQUEST_BOUNCER_HPP
#define CPPA_SYNC_REQUEST_BOUNCER_HPP

#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_id.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/mailbox_element.hpp"

namespace cppa { namespace detail {

struct sync_request_bouncer {
    std::uint32_t rsn;
    constexpr sync_request_bouncer(std::uint32_t r)
    : rsn(r == exit_reason::not_exited ? exit_reason::normal : r) { }
    inline void operator()(const actor_ptr& sender, const message_id& mid) const {
        CPPA_REQUIRE(rsn != exit_reason::not_exited);
        if (mid.is_request() && sender != nullptr) {
            actor_ptr nobody;
            sender->enqueue({nobody, mid.response_id()},
                            make_any_tuple(atom("EXITED"), rsn));
        }
    }
    inline void operator()(const mailbox_element& e) const {
        (*this)(e.sender.get(), e.mid);
    }
};

} } // namespace cppa::detail

#endif // CPPA_SYNC_REQUEST_BOUNCER_HPP
