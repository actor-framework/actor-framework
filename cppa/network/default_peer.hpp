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


#ifndef CPPA_DEFAULT_PEER_IMPL_HPP
#define CPPA_DEFAULT_PEER_IMPL_HPP

#include <map>
#include <cstdint>

#include "cppa/actor_proxy.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/weak_intrusive_ptr.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/network/input_stream.hpp"
#include "cppa/network/output_stream.hpp"
#include "cppa/network/continuable_io.hpp"
#include "cppa/network/default_message_queue.hpp"

namespace cppa { namespace network {

class default_protocol;

class default_peer : public continuable_io {

    typedef continuable_io super;

    friend class default_protocol;

 public:

    default_peer(default_protocol* parent,
                 const input_stream_ptr& in,
                 const output_stream_ptr& out,
                 process_information_ptr peer_ptr = nullptr);

    continue_reading_result continue_reading() override;

    continue_writing_result continue_writing() override;

    void io_failed();

    void enqueue(const message_header& hdr, const any_tuple& msg);

    inline bool erase_on_last_proxy_exited() const {
        return m_erase_on_last_proxy_exited;
    }

    inline const process_information& node() const {
        return *m_node;
    }

    inline bool has_unwritten_data() const {
        return m_has_unwritten_data;
    }

 protected:

    ~default_peer();

 private:

    void disconnected();

    enum read_state {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    default_protocol* m_parent;
    input_stream_ptr m_in;
    output_stream_ptr m_out;
    read_state m_state;
    process_information_ptr m_node;
    bool m_has_unwritten_data;

    const uniform_type_info* m_meta_hdr;
    const uniform_type_info* m_meta_msg;

    util::buffer m_rd_buf;
    util::buffer m_wr_buf;

    default_message_queue_ptr m_queue;

    inline default_message_queue& queue() {
        return *m_queue;
    }

    inline void set_queue(const default_message_queue_ptr& queue) {
        m_queue = queue;
    }

    // if this peer was created using remote_actor(), then m_doorman will
    // point to the published actor of the remote node
    bool m_erase_on_last_proxy_exited;

    partial_function m_content_handler;

    void monitor(const actor_ptr& sender, const process_information_ptr& node, actor_id aid);

    void kill_proxy(const actor_ptr& sender, const process_information_ptr& node, actor_id aid, std::uint32_t reason);

    void link(const actor_ptr& sender, const actor_ptr& ptr);

    void unlink(const actor_ptr& sender, const actor_ptr& ptr);

    void deliver(const message_header& hdr, any_tuple msg);

    inline void enqueue(const any_tuple& msg) {
        enqueue({nullptr, nullptr}, msg);
    }

};

typedef intrusive_ptr<default_peer> default_peer_ptr;

} } // namespace cppa::network

#endif // CPPA_DEFAULT_PEER_IMPL_HPP
