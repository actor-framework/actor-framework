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
#include <fcntl.h>
#include <cstdint>
#include <cstring>      // strerror
#include <unistd.h>
#include <iostream>
#include <sys/time.h>
#include <sys/types.h>

#include "cppa/detail/thread.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/post_office_msg.hpp"
#include "cppa/detail/network_manager.hpp"

namespace {

using namespace cppa;
using namespace cppa::detail;

struct network_manager_impl : network_manager
{

    typedef intrusive::single_reader_queue<post_office_msg> post_office_queue_t;
    typedef intrusive::single_reader_queue<mailman_job> mailman_queue_t;

    int m_pipe[2]; // m_pipe[0]: read; m_pipe[1]: write

    mailman_queue_t m_mailman_queue;
    post_office_queue_t m_post_office_queue;

    thread m_loop; // post office thread

    void start() /*override*/
    {
        if (pipe(m_pipe) != 0)
        {
            char* error_cstr = strerror(errno);
            std::string error_str = "pipe(): ";
            error_str += error_cstr;
            free(error_cstr);
            throw std::logic_error(error_str);
        }
        m_loop = thread(post_office_loop, m_pipe[0], m_pipe[1]);
    }

    void write_to_pipe(const pipe_msg& what)
    {
        if (write(m_pipe[1], what, pipe_msg_size) != (int) pipe_msg_size)
        {
            std::cerr << "FATAL: cannot write to pipe" << std::endl;
            abort();
        }
    }

    inline int write_handle() const
    {
        return m_pipe[1];
    }

    mailman_queue_t& mailman_queue()
    {
        return m_mailman_queue;
    }

    post_office_queue_t& post_office_queue()
    {
        return m_post_office_queue;
    }

    void stop() /*override*/
    {
        pipe_msg msg = { shutdown_event, 0 };
        write_to_pipe(msg);
        // m_loop calls close(m_pipe[0])
        m_loop.join();
        close(m_pipe[0]);
        close(m_pipe[1]);
    }

};

} // namespace <anonymous>

namespace cppa { namespace detail {

network_manager::~network_manager()
{
}

network_manager* network_manager::create_singleton()
{
    return new network_manager_impl;
}

} } // namespace cppa::detail
