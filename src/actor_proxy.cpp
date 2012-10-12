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


#include <utility>
#include <iostream>

#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/network/middleman.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/singleton_manager.hpp"

using namespace std;

namespace cppa {

namespace {

template<typename... Args>
void middleman_enqueue(Args&&... args) {
    detail::singleton_manager::get_middleman()->enqueue(forward<Args>(args)...);
}

} // namespace <anonymous>


actor_proxy::actor_proxy(std::uint32_t mid, const process_information_ptr& pptr)
: super(mid, pptr) { }

void actor_proxy::enqueue(actor* sender, any_tuple msg) {
    auto& arr = detail::static_types_array<atom_value, std::uint32_t>::arr;
    if (   msg.size() == 2
        && msg.type_at(0) == arr[0]
        && msg.get_as<atom_value>(0) == atom("KILL_PROXY")
        && msg.type_at(1) == arr[1]) {
        cleanup(msg.get_as<std::uint32_t>(1));
        return;
    }
    middleman_enqueue(parent_process_ptr(), sender, this, std::move(msg));
}

void actor_proxy::sync_enqueue(actor* sender, message_id_t id, any_tuple msg) {
    middleman_enqueue(parent_process_ptr(), sender, this, std::move(msg), id);
}

void actor_proxy::link_to(const intrusive_ptr<actor>& other) {
    if (link_to_impl(other)) {
        // causes remote actor to link to (proxy of) other
        middleman_enqueue(parent_process_ptr(),
                          other,
                          this,
                          make_any_tuple(atom("LINK"), other));
    }
}

void actor_proxy::local_link_to(const intrusive_ptr<actor>& other) {
    link_to_impl(other);
}

void actor_proxy::unlink_from(const intrusive_ptr<actor>& other) {
    if (unlink_from_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        middleman_enqueue(parent_process_ptr(),
                          other,
                          this,
                          make_any_tuple(atom("UNLINK"), other));
    }
}

void actor_proxy::local_unlink_from(const intrusive_ptr<actor>& other) {
    unlink_from_impl(other);
}

bool actor_proxy::establish_backlink(const intrusive_ptr<actor>& other) {
    bool result = super::establish_backlink(other);
    if (result) {
        // causes remote actor to unlink from (proxy of) other
        middleman_enqueue(parent_process_ptr(),
                          other,
                          this,
                          make_any_tuple(atom("LINK"), other));
    }
    return result;
}

bool actor_proxy::remove_backlink(const intrusive_ptr<actor>& other) {
    bool result = super::remove_backlink(other);
    if (result) {
        middleman_enqueue(parent_process_ptr(),
                          nullptr,
                          this,
                          make_any_tuple(atom("UNLINK"), actor_ptr(this)));
    }
    return result;
}

} // namespace cppa
