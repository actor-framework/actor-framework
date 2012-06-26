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


#ifndef CPPA_THREAD_BASED_ACTOR_HPP
#define CPPA_THREAD_BASED_ACTOR_HPP

#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <stack>
#include <atomic>
#include <vector>
#include <memory>
#include <cstdint>

#include "cppa/atom.hpp"
#include "cppa/either.hpp"
#include "cppa/pattern.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/detail/abstract_actor.hpp"

#include "cppa/intrusive/singly_linked_list.hpp"

#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/stacked_actor_mixin.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief An actor running in its own thread.
 */
class thread_mapped_actor : public local_actor {

 protected:

    /**
     * @brief Implements the actor's behavior.
     *        Reimplemented this function for a class-based actor.
     *        Returning from this member function will end the
     *        execution of the actor.
     */
    virtual void run();

};

#else // CPPA_DOCUMENTATION

class self_type;

class thread_mapped_actor : public detail::stacked_actor_mixin<
                                       thread_mapped_actor,
                                       detail::abstract_actor<local_actor> > {

    friend class self_type; // needs access to cleanup()
    friend class detail::receive_policy;

    typedef detail::stacked_actor_mixin<
                thread_mapped_actor,
                detail::abstract_actor<local_actor> > super;

 public:

    thread_mapped_actor();

    thread_mapped_actor(std::function<void()> fun);

    void quit(std::uint32_t reason = exit_reason::normal); //override

    void enqueue(actor* sender, any_tuple msg); //override

    detail::filter_result filter_msg(const any_tuple& msg);

    inline decltype(m_mailbox)& mailbox() { return m_mailbox; }

    inline void initialized(bool value) { m_initialized = value; }

 protected:

    bool initialized();

 private:

    bool m_initialized;

    // required by nestable_receive_policy
    static const detail::receive_policy_flag receive_flag = detail::rp_nestable;
    inline void push_timeout() { }
    inline void pop_timeout() { }
    inline detail::recursive_queue_node* receive_node() {
        return m_mailbox.pop();
    }
    inline auto init_timeout(const util::duration& tout) -> decltype(std::chrono::high_resolution_clock::now()) {
        auto result = std::chrono::high_resolution_clock::now();
        result += tout;
        return result;
    }
    inline detail::recursive_queue_node* try_receive_node() {
        return m_mailbox.try_pop();
    }
    template<typename Timeout>
    inline detail::recursive_queue_node* try_receive_node(const Timeout& tout) {
        return m_mailbox.try_pop(tout);
    }
    inline void handle_timeout(behavior& bhvr) {
        bhvr.handle_timeout();
    }

};

typedef intrusive_ptr<thread_mapped_actor> thread_mapped_actor_ptr;

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // CPPA_THREAD_BASED_ACTOR_HPP
