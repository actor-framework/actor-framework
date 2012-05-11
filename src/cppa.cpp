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


#include "cppa/cppa.hpp"
#include "cppa/local_actor.hpp"

namespace cppa {

namespace {

class down_observer : public attachable {

    actor_ptr m_observer;
    actor_ptr m_observed;

 public:

    down_observer(actor_ptr observer, actor_ptr observed)
        : m_observer(std::move(observer))
        , m_observed(std::move(observed)) {
    }

    void actor_exited(std::uint32_t reason) {
        using namespace cppa;
        send(m_observer, atom("DOWN"), m_observed, reason);
    }

    bool matches(const attachable::token& match_token) {
        if (match_token.subtype == typeid(down_observer)) {
            auto ptr = reinterpret_cast<const local_actor*>(match_token.ptr);
            return m_observer == ptr;
        }
        return false;
    }

};

} // namespace <anonymous>

const self_type& operator<<(const self_type& s, const any_tuple& what) {
    local_actor* sptr = s;
    sptr->enqueue(sptr, what);
    return s;
}

const self_type& operator<<(const self_type& s, any_tuple&& what) {
    local_actor* sptr = s;
    sptr->enqueue(sptr, std::move(what));
    return s;
}

void link(actor_ptr& other) {
    if (other) self->link_to(other);
}

void link(actor_ptr&& other) {
    actor_ptr tmp(std::move(other));
    link(tmp);
}

void link(actor_ptr& lhs, actor_ptr& rhs) {
    if (lhs && rhs) lhs->link_to(rhs);
}

void link(actor_ptr&& lhs, actor_ptr& rhs) {
    actor_ptr tmp(std::move(lhs));
    link(tmp, rhs);
}

void link(actor_ptr&& lhs, actor_ptr&& rhs) {
    actor_ptr tmp1(std::move(lhs));
    actor_ptr tmp2(std::move(rhs));
    link(tmp1, tmp2);
}

void link(actor_ptr& lhs, actor_ptr&& rhs) {
    actor_ptr tmp(std::move(rhs));
    link(lhs, tmp);
}

void unlink(actor_ptr& lhs, actor_ptr& rhs) {
    if (lhs && rhs) lhs->unlink_from(rhs);
}

void monitor(actor_ptr& whom) {
    if (whom) whom->attach(new down_observer(self, whom));
}

void monitor(actor_ptr&& whom) {
    actor_ptr tmp(std::move(whom));
    monitor(tmp);
}

void demonitor(actor_ptr& whom) {
    attachable::token mtoken(typeid(down_observer), self);
    if (whom) whom->detach(mtoken);
}

} // namespace cppa
