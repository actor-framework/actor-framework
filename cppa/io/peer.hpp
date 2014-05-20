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


#ifndef CPPA_peer_IMPL_HPP
#define CPPA_peer_IMPL_HPP

#include <map>
#include <cstdint>

#include "cppa/extend.hpp"
#include "cppa/node_id.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/type_lookup_table.hpp"
#include "cppa/weak_intrusive_ptr.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/io/input_stream.hpp"
#include "cppa/io/output_stream.hpp"
#include "cppa/io/buffered_writing.hpp"
#include "cppa/io/default_message_queue.hpp"

namespace cppa {
namespace io {

class middleman_impl;

class peer : public extend<continuable>::with<buffered_writing> {

    typedef combined_type super;

    friend class middleman_impl;

 public:

    peer(middleman* parent,
         const input_stream_ptr& in,
         const output_stream_ptr& out,
         node_id_ptr peer_ptr = nullptr);

    continue_reading_result continue_reading() override;

    continue_writing_result continue_writing() override;

    void dispose() override;

    void io_failed(event_bitmask mask) override;

    void enqueue(msg_hdr_cref hdr, const any_tuple& msg);

    inline bool stop_on_last_proxy_exited() const {
        return m_stop_on_last_proxy_exited;
    }

    inline const node_id& node() const {
        return *m_node;
    }

 private:

    enum read_state {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    input_stream_ptr m_in;
    read_state m_state;
    node_id_ptr m_node;

    const uniform_type_info* m_meta_hdr;
    const uniform_type_info* m_meta_msg;

    util::buffer m_rd_buf;
    util::buffer m_wr_buf;

    default_message_queue_ptr m_queue;

    inline default_message_queue& queue() {
        CPPA_REQUIRE(m_queue != nullptr);
        return *m_queue;
    }

    inline void set_queue(const default_message_queue_ptr& queue) {
        m_queue = queue;
    }

    // if this peer was created using remote_actor(), then m_doorman will
    // point to the published actor of the remote node
    bool m_stop_on_last_proxy_exited;

    partial_function m_content_handler;

    type_lookup_table m_incoming_types;
    type_lookup_table m_outgoing_types;

    void monitor(const actor_addr& sender, const node_id_ptr& node, actor_id aid);

    void kill_proxy(const actor_addr& sender, const node_id_ptr& node, actor_id aid, std::uint32_t reason);

    void link(const actor_addr& sender, const actor_addr& ptr);

    void unlink(const actor_addr& sender, const actor_addr& ptr);

    void deliver(msg_hdr_cref hdr, any_tuple msg);

    inline void enqueue(const any_tuple& msg) {
        enqueue({invalid_actor_addr, nullptr}, msg);
    }

    void enqueue_impl(msg_hdr_cref hdr, const any_tuple& msg);

    void add_type_if_needed(const std::string& tname);

};

} // namespace io
} // namespace cppa


#endif // CPPA_peer_IMPL_HPP
