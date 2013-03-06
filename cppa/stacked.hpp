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

#include <chrono>
#include <memory>
#include <functional>

#include "cppa/behavior.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa {

/**
 * @tparam Base Either @p scheduled or @p threaded.
 */
template<class Base, class Subtype>
class stacked : public Base {

    friend class detail::receive_policy;
    friend class detail::behavior_stack;

 protected:

    typedef stacked combined_type;

 public:

    static constexpr auto receive_flag = detail::rp_nestable;

    virtual void dequeue(behavior& bhvr) {
        m_recv_policy.receive(dthis(), bhvr);
    }

    virtual void dequeue_response(behavior& bhvr, message_id request_id) {
        m_recv_policy.receive(dthis(), bhvr, request_id);
    }

    virtual void run() {
        if (!dthis()->m_bhvr_stack.empty()) {
            dthis()->exec_behavior_stack();
        }
        if (m_behavior) {
            m_behavior();
        }
    }

    inline void set_behavior(std::function<void()> fun) {
        m_behavior = std::move(fun);
    }

    virtual void quit(std::uint32_t reason) {
        this->cleanup(reason);
        throw actor_exited(reason);
    }

 protected:

    template<typename... Args>
    stacked(std::function<void()> fun, Args&&... args)
    : Base(std::forward<Args>(args)...), m_behavior(std::move(fun)) { }

    virtual void do_become(behavior&& bhvr, bool discard_old) {
        become_impl(std::move(bhvr), discard_old, message_id());
    }

    virtual void become_waiting_for(behavior bhvr, message_id mid) {
        become_impl(std::move(bhvr), false, mid);
    }

    virtual bool has_behavior() {
        return static_cast<bool>(m_behavior) || !dthis()->m_bhvr_stack.empty();
    }

    typedef std::chrono::time_point<std::chrono::high_resolution_clock>
            timeout_type;

    virtual timeout_type init_timeout(const util::duration& rel_time) = 0;

    virtual detail::recursive_queue_node* await_message() = 0;

    virtual detail::recursive_queue_node* await_message(const timeout_type& abs_time) = 0;

 private:

    std::function<void()> m_behavior;
    detail::receive_policy m_recv_policy;

    void become_impl(behavior&& bhvr, bool discard_old, message_id mid) {
        if (bhvr.timeout().valid()) {
            dthis()->reset_timeout();
            dthis()->request_timeout(bhvr.timeout());
        }
        if (!dthis()->m_bhvr_stack.empty() && discard_old) {
            dthis()->m_bhvr_stack.pop_async_back();
        }
        dthis()->m_bhvr_stack.push_back(std::move(bhvr), mid);
    }

    virtual void exec_behavior_stack() {
        this->m_bhvr_stack.exec(m_recv_policy, dthis());
    }

    inline Subtype* dthis() { return static_cast<Subtype*>(this); }

};

} // namespace cppa

#endif // CPPA_STACKED_ACTOR_MIXIN_HPP
