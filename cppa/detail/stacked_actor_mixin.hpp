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

#include "cppa/behavior.hpp"

#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

template<class Derived, class Base>
class stacked_actor_mixin : public Base {

    inline Derived* dthis() {
        return static_cast<Derived*>(this);
    }

 public:

    virtual void unbecome() {
        if (m_bhvr_stack_ptr) {
            m_bhvr_stack_ptr->pop_back();
        }
        else if (this->initialized()) {
            this->quit();
        }
    }

    virtual void dequeue(partial_function& fun) {
        m_recv_policy.receive(dthis(), fun);
    }

    virtual void dequeue(behavior& bhvr) {
        m_recv_policy.receive(dthis(), bhvr);
    }

 protected:

    receive_policy m_recv_policy;
    std::unique_ptr<behavior_stack> m_bhvr_stack_ptr;

    virtual void do_become(behavior* bhvr, bool owns_bhvr, bool discard_old) {
        if (this->m_bhvr_stack_ptr) {
            if (discard_old) this->m_bhvr_stack_ptr->pop_back();
            this->m_bhvr_stack_ptr->push_back(bhvr, owns_bhvr);
        }
        else {
            this->m_bhvr_stack_ptr.reset(new behavior_stack);
            if (this->initialized()) {
                this->m_bhvr_stack_ptr->exec();
                this->quit();
            }
        }
    }

};

} } // namespace cppa::detail

#endif // CPPA_STACKED_ACTOR_MIXIN_HPP
