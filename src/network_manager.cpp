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


#include <cstdio>
#include <thread>
#include <fcntl.h>
#include <cstdint>
#include <cstring>      // strerror
#include <unistd.h>
#include <iostream>
#include <sys/time.h>
#include <sys/types.h>

#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace {

using namespace cppa;
using namespace cppa::detail;

struct network_manager_impl : network_manager {

    local_actor_ptr m_mailman;
    std::thread m_mailman_thread;

    local_actor_ptr m_post_office;
    std::thread m_post_office_thread;

    int pipe_fd[2];

    void start() { // override
        if (pipe(pipe_fd) != 0) {
            CPPA_CRITICAL("cannot create pipe");
        }

        m_post_office.reset(new converted_thread_context);
        m_post_office_thread = mock_scheduler::spawn_hidden_impl(std::bind(post_office_loop, pipe_fd[0]), m_post_office);

        m_mailman.reset(new converted_thread_context);
        m_mailman_thread = mock_scheduler::spawn_hidden_impl(mailman_loop, m_mailman);
    }

    void stop() { // override
        m_mailman->enqueue(nullptr, make_any_tuple(atom("DONE")));
        m_mailman_thread.join();
        // wait until mailman is done; post_office closes all sockets
        std::atomic_thread_fence(std::memory_order_seq_cst);
        send_to_post_office(po_message{atom("DONE"), -1, 0});
        m_post_office_thread.join();
        close(pipe_fd[0]);
        close(pipe_fd[0]);
    }

    void send_to_post_office(const po_message& msg) {
        if (write(pipe_fd[1], &msg, sizeof(po_message)) != sizeof(po_message)) {
            CPPA_CRITICAL("cannot write to pipe");
        }
    }

    void send_to_post_office(any_tuple msg) {
        m_post_office->enqueue(nullptr, std::move(msg));
    }

    void send_to_mailman(any_tuple msg) {
        m_mailman->enqueue(nullptr, std::move(msg));
    }

};

} // namespace <anonymous>

namespace cppa { namespace detail {

network_manager::~network_manager() {
}

network_manager* network_manager::create_singleton() {
    return new network_manager_impl;
}

} } // namespace cppa::detail
