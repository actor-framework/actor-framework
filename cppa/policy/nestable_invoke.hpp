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
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_THREADED_HPP
#define CPPA_THREADED_HPP

#include <mutex>
#include <chrono>
#include <condition_variable>

#include "cppa/exit_reason.hpp"
#include "cppa/mailbox_element.hpp"

#include "cppa/util/dptr.hpp"

#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/policy/invoke_policy.hpp"

namespace cppa { namespace detail { class receive_policy; } }

namespace cppa { namespace policy {

class nestable_invoke : public invoke_policy<nestable_invoke> {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

     static inline bool hm_should_skip(pointer) {
        return false;
    }

    template<class Client>
    static inline pointer hm_begin(Client* client, pointer node) {
        auto previous = client->current_node();
        client->current_node(node);
        return previous;
    }

    template<class Client>
    static inline void hm_cleanup(Client* client, pointer /*previous*/) {
        client->current_node(&(client->m_dummy_node));
    }

    template<class Client>
    static inline void hm_revert(Client* client, pointer previous) {
        client->current_node(previous);
    }

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

    inline void reset_timeout() { }

    template<class Actor>
    inline void request_timeout(Actor*, const util::duration&) { }

    template<class Actor>
    inline void handle_timeout(Actor*, behavior& bhvr) {
        bhvr.handle_timeout();
    }

    inline void pop_timeout() { }

    inline void push_timeout() { }

    inline bool waits_for_timeout(std::uint32_t) { return false; }

    /*
    void run_detached() {
        auto dthis = util::dptr<Subtype>(this);
        dthis->init();
        if (dthis->planned_exit_reason() != exit_reason::not_exited) {
            // init() did indeed call quit() for some reason
            dthis->on_exit();
        }
        auto rsn = dthis->planned_exit_reason();
        dthis->cleanup(rsn == exit_reason::not_exited ? exit_reason::normal : rsn);
    }
    */

};

} } // namespace cppa::policy

#endif // CPPA_THREADED_HPP
