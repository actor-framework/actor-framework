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


#ifndef MAILMAN_HPP
#define MAILMAN_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace detail {

struct mailman_send_job
{
    process_information_ptr target_peer;
    addressed_message msg;
    inline mailman_send_job(process_information_ptr piptr,
                            actor_ptr const& from,
                            channel_ptr const& to,
                            any_tuple const& content)
        : target_peer(piptr), msg(from, to, content)
    {
    }
};

struct mailman_add_peer
{
    native_socket_type sockfd;
    process_information_ptr pinfo;
    inline mailman_add_peer(native_socket_type fd,
                            process_information_ptr const& piptr)
        : sockfd(fd), pinfo(piptr)
    {
    }
};

class mailman_job
{

    friend class intrusive::singly_linked_list<mailman_job>;
    friend class intrusive::single_reader_queue<mailman_job>;

 public:

    enum job_type
    {
        invalid_type,
        send_job_type,
        add_peer_type,
        kill_type
    };

    inline mailman_job() : next(nullptr), m_type(invalid_type) { }

    mailman_job(process_information_ptr piptr,
                actor_ptr const& from,
                channel_ptr const& to,
                any_tuple const& omsg);

    mailman_job(native_socket_type sockfd, process_information_ptr const& pinfo);

    static mailman_job* kill_job();

    ~mailman_job();

    inline mailman_send_job& send_job()
    {
        return m_send_job;
    }

    inline mailman_add_peer& add_peer_job()
    {
        return m_add_socket;
    }

    inline job_type type() const
    {
        return m_type;
    }

    inline bool is_send_job() const
    {
        return m_type == send_job_type;
    }

    inline bool is_add_peer_job() const
    {
        return m_type == add_peer_type;
    }

    inline bool is_kill_job() const
    {
        return m_type == kill_type;
    }

 private:

    mailman_job* next;
    job_type m_type;
    // unrestricted union
    union
    {
        mailman_send_job m_send_job;
        mailman_add_peer m_add_socket;
    };

    inline mailman_job(job_type jt) : next(nullptr), m_type(jt) { }

};

void mailman_loop();

intrusive::single_reader_queue<mailman_job>& mailman_queue();

}} // namespace cppa::detail

#endif // MAILMAN_HPP
