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
 * Copyright (C) 2011, 2012                                                   *
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


#include <chrono>
#include <memory>
#include <iostream>
#include <algorithm>

#include "cppa/self.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/thread_mapped_actor.hpp"

#include "cppa/detail/matches.hpp"

namespace cppa {

void thread_mapped_actor::quit(std::uint32_t reason) {
    super::cleanup(reason);
    // actor_exited should not be catched, but if anyone does,
    // self must point to a newly created instance
    //self.set(nullptr);
    throw actor_exited(reason);
}

void thread_mapped_actor::enqueue(actor* sender, any_tuple msg) {
    auto node = fetch_node(sender, std::move(msg));
    CPPA_REQUIRE(node->marked == false);
    m_mailbox.push_back(node);
}

void thread_mapped_actor::dequeue(partial_function& fun) { // override
    m_recv_policy.receive(this, fun);
}

void thread_mapped_actor::dequeue(behavior& bhvr) { // override
    auto& fun = bhvr.get_partial_function();
    if (bhvr.timeout().valid() == false) {
        // suppress virtual function call
        thread_mapped_actor::dequeue(fun);
    }
    else if (m_recv_policy.invoke_from_cache(this, fun) == false) {
        if (bhvr.timeout().is_zero()) {
            for (auto e = m_mailbox.try_pop(); e != nullptr; e = m_mailbox.try_pop()) {
                CPPA_REQUIRE(e->marked == false);
                if (m_recv_policy.invoke(this, e, bhvr)) return;
                e = m_mailbox.try_pop();
            }
            bhvr.handle_timeout();
        }
        else {
            auto timeout = std::chrono::high_resolution_clock::now();
            timeout += bhvr.timeout();
            detail::recursive_queue_node* e = m_mailbox.try_pop(timeout);
            while (e != nullptr) {
                CPPA_REQUIRE(e->marked == false);
                if (m_recv_policy.invoke(this, e, fun)) return;
                e = m_mailbox.try_pop(timeout);
            }
            bhvr.handle_timeout();
        }
    }
}

detail::filter_result thread_mapped_actor::filter_msg(const any_tuple& msg) {
    auto& arr = detail::static_types_array<atom_value, std::uint32_t>::arr;
    if (   m_trap_exit == false
        && msg.size() == 2
        && msg.type_at(0) == arr[0]
        && msg.type_at(1) == arr[1]) {
        auto v0 = msg.get_as<atom_value>(0);
        auto v1 = msg.get_as<std::uint32_t>(1);
        if (v0 == atom("EXIT")) {
            if (v1 != exit_reason::normal) {
                quit(v1);
            }
            return detail::normal_exit_signal;
        }

    }
    return detail::ordinary_message;
}

} // namespace cppa::detail
