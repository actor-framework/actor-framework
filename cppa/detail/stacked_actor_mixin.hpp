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


#ifndef CPPA_STACKED_ACTOR_MIXIN_HPP
#define CPPA_STACKED_ACTOR_MIXIN_HPP

#include <memory>
#include <functional>

#include "cppa/behavior.hpp"

#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

template<class Derived, class Base>
class stacked_actor_mixin : public Base {

 public:

    virtual void unbecome() {
        if (m_bhvr_stack_ptr) {
            m_bhvr_stack_ptr->pop_back();
        }
    }

    virtual void dequeue(partial_function& fun) {
        m_recv_policy.receive(dthis(), fun);
    }

    virtual void dequeue(behavior& bhvr) {
        m_recv_policy.receive(dthis(), bhvr);
    }

    virtual void run() {
        if (m_bhvr_stack_ptr) {
            m_bhvr_stack_ptr->exec();
            m_bhvr_stack_ptr.reset();
        }
        if (m_behavior) {
            m_behavior();
        }
    }

 protected:

    stacked_actor_mixin() = default;

    stacked_actor_mixin(std::function<void()> f) : m_behavior(std::move(f)) { }

    virtual void do_become(behavior* bhvr, bool owns_bhvr, bool discard_old) {
        if (m_bhvr_stack_ptr) {
            if (discard_old) m_bhvr_stack_ptr->pop_back();
            m_bhvr_stack_ptr->push_back(bhvr, owns_bhvr);
        }
        else {
            m_bhvr_stack_ptr.reset(new behavior_stack);
            m_bhvr_stack_ptr->push_back(bhvr, owns_bhvr);
            if (this->initialized()) {
                m_bhvr_stack_ptr->exec();
                m_bhvr_stack_ptr.reset();
            }
        }
    }

    virtual bool has_behavior() {
        return     static_cast<bool>(m_behavior)
                || (   static_cast<bool>(m_bhvr_stack_ptr)
                    && m_bhvr_stack_ptr->empty() == false);
    }

 private:

    std::function<void()> m_behavior;
    receive_policy m_recv_policy;
    std::unique_ptr<behavior_stack> m_bhvr_stack_ptr;

    inline Derived* dthis() {
        return static_cast<Derived*>(this);
    }

};

} } // namespace cppa::detail

#endif // CPPA_STACKED_ACTOR_MIXIN_HPP
