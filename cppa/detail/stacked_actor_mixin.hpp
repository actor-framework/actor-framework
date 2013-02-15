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


#ifndef CPPA_STACKED_ACTOR_MIXIN_HPP
#define CPPA_STACKED_ACTOR_MIXIN_HPP

#include <memory>
#include <functional>

#include "cppa/behavior.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

template<class Derived, class Base>
class stacked_actor_mixin : public Base {

    friend class receive_policy;

 public:

    virtual void dequeue(partial_function& fun) {
        m_recv_policy.receive(dthis(), fun);
    }

    virtual void dequeue(behavior& bhvr) {
        m_recv_policy.receive(dthis(), bhvr);
    }

    virtual void dequeue_response(behavior& bhvr, message_id_t request_id) {
        m_recv_policy.receive(dthis(), bhvr, request_id);
    }

    virtual void run() {
        if (!this->m_bhvr_stack.empty()) {
            this->m_bhvr_stack.exec(m_recv_policy, dthis());
        }
        if (m_behavior) {
            m_behavior();
        }
    }

 protected:

    stacked_actor_mixin() = default;

    stacked_actor_mixin(std::function<void()> f) : m_behavior(std::move(f)) { }

    virtual void do_become(behavior&& bhvr, bool discard_old) {
        become_impl(std::move(bhvr), discard_old, message_id_t());
    }

    virtual void become_waiting_for(behavior&& bhvr, message_id_t mid) {
        become_impl(std::move(bhvr), false, mid);
    }

    virtual bool has_behavior() {
        return     static_cast<bool>(m_behavior)
                || !this->m_bhvr_stack.empty();
    }

 private:

    std::function<void()> m_behavior;
    receive_policy m_recv_policy;

    inline Derived* dthis() {
        return static_cast<Derived*>(this);
    }

    void become_impl(behavior&& bhvr, bool discard_old, message_id_t mid) {
        if (bhvr.timeout().valid()) {
            dthis()->reset_timeout();
            dthis()->request_timeout(bhvr.timeout());
        }
        if (!this->m_bhvr_stack.empty() && discard_old) {
            this->m_bhvr_stack.pop_async_back();
        }
        this->m_bhvr_stack.push_back(std::move(bhvr), mid);
    }

    virtual void exec_behavior_stack() {
        this->m_bhvr_stack.exec(m_recv_policy, dthis());
    }

};

} } // namespace cppa::detail

#endif // CPPA_STACKED_ACTOR_MIXIN_HPP
