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


#ifndef IPV4_PEER_HPP
#define IPV4_PEER_HPP

#include "cppa/process_information.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/network/peer.hpp"
#include "cppa/network/input_stream.hpp"
#include "cppa/network/output_stream.hpp"

namespace cppa { namespace network {

class default_peer_impl : public peer {

    typedef peer super;

 public:

    default_peer_impl(middleman* parent,
                      const input_stream_ptr& in,
                      const output_stream_ptr& out,
                      process_information_ptr peer_ptr = nullptr);

    continue_reading_result continue_reading();

    continue_writing_result continue_writing();

    bool enqueue(const addressed_message& msg);

 protected:

    ~default_peer_impl();

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
    output_stream_ptr m_out;
    read_state m_state;
    process_information_ptr m_peer;
    const uniform_type_info* m_meta_msg;
    bool m_has_unwritten_data;

    util::buffer m_rd_buf;
    util::buffer m_wr_buf;

};

} } // namespace cppa::network

#endif // IPV4_PEER_HPP
