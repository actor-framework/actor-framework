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
 * Free Software Foundation, either version 3 of the License                  *
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


#include <utility>

#include "cppa/self.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/response_handle.hpp"

using std::move;

namespace cppa {

response_handle::response_handle(const actor_ptr&    from,
                                 const actor_ptr&    to,
                                 const message_id_t& id)
: m_from(from), m_to(to), m_id(id) {
    CPPA_REQUIRE(id.is_response());
}

bool response_handle::valid() const {
    return m_to != nullptr;
}

bool response_handle::synchronous() const {
    return m_id.valid();
}

void response_handle::apply(any_tuple msg) const {
    if (valid()) {
        local_actor* sptr = self.unchecked();
        if (sptr && sptr == m_from) {
            if (sptr->chaining_enabled()) {
                if (m_to->chained_sync_enqueue(sptr, m_id, move(msg))) {
                    sptr->chained_actor(m_to);
                }
            }
        }
        else m_to->enqueue(m_from.get(), move(msg));
    }
}

} // namespace cppa
