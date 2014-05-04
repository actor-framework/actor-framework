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


#ifndef CPPA_BROKER_HPP
#define CPPA_BROKER_HPP

#include <map>

#include "cppa/extend.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/behavior_stack_based.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/io/stream.hpp"
#include "cppa/io/acceptor.hpp"
#include "cppa/io/input_stream.hpp"
#include "cppa/io/tcp_acceptor.hpp"
#include "cppa/io/tcp_io_stream.hpp"
#include "cppa/io/output_stream.hpp"
#include "cppa/io/accept_handle.hpp"
#include "cppa/io/connection_handle.hpp"

#include "cppa/policy/not_prioritizing.hpp"
#include "cppa/policy/sequential_invoke.hpp"

namespace cppa {
namespace io {

class broker;

typedef intrusive_ptr<broker> broker_ptr;

broker_ptr init_and_launch(broker_ptr);

/**
 * @brief A broker mediates between a libcppa-based actor system
 *        and other components in the network.
 * @extends local_actor
 */
class broker : public extend<local_actor>::
                      with<behavior_stack_based<behavior>::impl> {

    friend class policy::sequential_invoke;

    typedef combined_type super;

    // implementation relies on several helper classes ...
    class scribe;
    class servant;
    class doorman;
    class continuation;

    // ... and some helpers need friendship
    friend class scribe;
    friend class doorman;
    friend class continuation;

    friend broker_ptr init_and_launch(broker_ptr);

    broker() = delete;

 public:

    ~broker();

    enum policy_flag { at_least, at_most, exactly };

    void enqueue(msg_hdr_cref, any_tuple, execution_unit*) override;

    bool initialized() const;

    void receive_policy(const connection_handle& hdl,
                        broker::policy_flag policy,
                        size_t buffer_size);

    void write(const connection_handle& hdl, size_t num_bytes, const void* buf);

    void write(const connection_handle& hdl, const util::buffer& buf);

    void write(const connection_handle& hdl, util::buffer&& buf);

    template<typename F, typename... Ts>
    static broker_ptr from(F fun,
                           input_stream_ptr in,
                           output_stream_ptr out,
                           Ts&&... args) {
        auto hdl = connection_handle::from_int(in->read_handle());
        return from_impl(std::bind(std::move(fun),
                                   std::placeholders::_1,
                                   hdl,
                                   std::forward<Ts>(args)...),
                         std::move(in),
                         std::move(out));
    }

    static broker_ptr from(std::function<void (broker*)> fun, acceptor_uptr in);

    static broker_ptr from(std::function<behavior (broker*)> fun, acceptor_uptr in);

    template<typename F, typename T0, typename... Ts>
    static broker_ptr from(F fun, acceptor_uptr in, T0&& arg0, Ts&&... args) {
        return from(std::bind(std::move(fun),
                              std::placeholders::_1,
                              std::forward<T0>(arg0),
                              std::forward<Ts>(args)...),
                    std::move(in));
    }

    template<typename F, typename... Ts>
    actor fork(F fun, connection_handle hdl, Ts&&... args) {
        return this->fork_impl(std::bind(std::move(fun),
                                         std::placeholders::_1,
                                         hdl,
                                         std::forward<Ts>(args)...),
                               hdl);
    }

    template<typename F>
    inline void for_each_connection(F fun) const {
        for (auto& kvp : m_io) fun(kvp.first);
    }

    inline size_t num_connections() const {
        return m_io.size();
    }

    connection_handle add_connection(input_stream_ptr in, output_stream_ptr out);

    inline connection_handle add_connection(stream_ptr sptr) {
        return add_connection(sptr, sptr);
    }

    inline connection_handle add_tcp_connection(native_socket_type tcp_sockfd) {
        return add_connection(tcp_io_stream::from_sockfd(tcp_sockfd));
    }

    accept_handle add_acceptor(acceptor_uptr ptr);

    inline accept_handle add_tcp_acceptor(native_socket_type tcp_sockfd) {
        return add_acceptor(tcp_acceptor::from_sockfd(tcp_sockfd));
    }

 protected:

    broker(input_stream_ptr in, output_stream_ptr out);

    broker(acceptor_uptr in);

    void cleanup(std::uint32_t reason) override;

    typedef std::unique_ptr<broker::scribe> scribe_pointer;

    typedef std::unique_ptr<broker::doorman> doorman_pointer;

    explicit broker(scribe_pointer);

    virtual behavior make_behavior() = 0;

 private:

    actor fork_impl(std::function<void (broker*)> fun,
                    connection_handle hdl);

    actor fork_impl(std::function<behavior (broker*)> fun,
                    connection_handle hdl);

    static broker_ptr from_impl(std::function<void (broker*)> fun,
                                input_stream_ptr in,
                                output_stream_ptr out);

    static broker_ptr from_impl(std::function<behavior (broker*)> fun,
                                input_stream_ptr in,
                                output_stream_ptr out);

    void invoke_message(msg_hdr_cref hdr, any_tuple msg);

    bool invoke_message_from_cache();

    void erase_io(int id);

    void erase_acceptor(int id);

    void init_broker();

    std::map<accept_handle, doorman_pointer> m_accept;
    std::map<connection_handle, scribe_pointer> m_io;

    policy::not_prioritizing  m_priority_policy;
    policy::sequential_invoke m_invoke_policy;

};

class default_broker : public broker {

 public:

    typedef std::function<behavior (broker*)> function_type;

    default_broker(function_type f, input_stream_ptr in, output_stream_ptr out);

    default_broker(function_type f, scribe_pointer ptr);

    default_broker(function_type f, acceptor_uptr ptr);

    behavior make_behavior() override;

 private:

    function_type m_fun;

};

} // namespace io
} // namespace cppa

#endif // CPPA_BROKER_HPP
