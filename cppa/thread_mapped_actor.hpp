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
#include "cppa/extend.hpp"
#include "cppa/stacked.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/mailbox_based.hpp"

#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

#include "cppa/intrusive/blocking_single_reader_queue.hpp"

namespace cppa {

class self_type;
class scheduler_helper;
class thread_mapped_actor;

template<>
struct has_blocking_receive<thread_mapped_actor> : std::true_type { };

/**
 * @brief An actor running in its own thread.
 * @extends local_actor
 */
class thread_mapped_actor : public extend<local_actor,thread_mapped_actor>::with<stacked,mailbox_based> {

    friend class self_type;              // needs access to cleanup()
    friend class scheduler_helper;       // needs access to mailbox
    friend class detail::receive_policy; // needs access to await_message(), etc.
    friend class detail::behavior_stack; // needs same access as receive_policy

    typedef combined_type super;

 public:

    thread_mapped_actor();

    thread_mapped_actor(std::function<void()> fun);

    inline void initialized(bool value) { m_initialized = value; }

    bool initialized() const;

    void enqueue(const actor_ptr& sender, any_tuple msg);

    void sync_enqueue(const actor_ptr& sender, message_id_t id, any_tuple msg);

    inline void reset_timeout() { }
    inline void request_timeout(const util::duration&) { }
    inline void handle_timeout(behavior& bhvr) { bhvr.handle_timeout(); }
    inline void pop_timeout() { }
    inline void push_timeout() { }
    inline bool waits_for_timeout(std::uint32_t) { return false; }

 protected:

    typedef detail::recursive_queue_node mailbox_element;

    typedef intrusive::blocking_single_reader_queue<mailbox_element,detail::disposer>
            mailbox_type;

    void cleanup(std::uint32_t reason);

 private:

    timeout_type init_timeout(const util::duration& rel_time);

    detail::recursive_queue_node* await_message();

    detail::recursive_queue_node* await_message(const timeout_type& abs_time);

    bool m_initialized;

 protected:

    mailbox_type m_mailbox;

};

typedef intrusive_ptr<thread_mapped_actor> thread_mapped_actor_ptr;

} // namespace cppa

#endif // CPPA_THREAD_BASED_ACTOR_HPP
