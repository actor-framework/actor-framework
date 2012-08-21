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
#include <cstdint>
#include <cstring>      // strerror
#include <unistd.h>
#include <iostream>
#include <sys/time.h>
#include <sys/types.h>

#include "cppa/self.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/thread_mapped_actor.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/detail/fd_util.hpp"
#include "cppa/detail/middleman.hpp"
#include "cppa/detail/network_manager.hpp"

namespace {

using namespace cppa;
using namespace cppa::detail;

struct network_manager_impl : network_manager {

    middleman_queue m_middleman_queue;
    std::thread m_middleman_thread;

    int pipe_fd[2];

    void start() { // override
        if (pipe(pipe_fd) != 0) {
            CPPA_CRITICAL("cannot create pipe");
        }
        // store pipe read handle in local variables for lambda expression
        int pipe_fd0 = pipe_fd[0];
        fd_util::nonblocking(pipe_fd0, true);
        // start threads
        m_middleman_thread = std::thread([this, pipe_fd0] {
            middleman_loop(pipe_fd0, this->m_middleman_queue);
        });
    }

    void stop() { // override
        //m_mailman->enqueue(nullptr, make_any_tuple(atom("DONE")));
        send_to_middleman(middleman_message::create());
        m_middleman_thread.join();
        close(pipe_fd[0]);
        close(pipe_fd[1]);
    }

    void send_to_middleman(std::unique_ptr<middleman_message> msg) {
        m_middleman_queue._push_back(msg.release());
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::uint8_t dummy = 0;
        if (write(pipe_fd[1], &dummy, sizeof(dummy)) != sizeof(dummy)) {
            CPPA_CRITICAL("cannot write to pipe");
        }
    }

};

} // namespace <anonymous>

namespace cppa { namespace detail {

network_manager::~network_manager() { }

network_manager* network_manager::create_singleton() {
    return new network_manager_impl;
}

} } // namespace cppa::detail
