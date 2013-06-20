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


#ifndef IO_ACTOR_BACKEND_HPP
#define IO_ACTOR_BACKEND_HPP

#include <cstdint>

#include "cppa/io/broker.hpp"
#include "cppa/io/io_handle.hpp"
#include "cppa/io/input_stream.hpp"
#include "cppa/io/output_stream.hpp"
#include "cppa/io/buffered_writer.hpp"

namespace cppa { namespace io {

class broker_backend : public buffered_writer, public io_handle {

    typedef buffered_writer super; // io_service is merely an interface

    // 65k is the maximum TCP package size
    static constexpr size_t default_max_buffer_size = 65535;

 public:

    broker_backend(input_stream_ptr in, output_stream_ptr out, broker_ptr ptr);

    void init();

    void handle_disconnect();

    void io_failed() override;

    void receive_policy(policy_flag policy, size_t buffer_size) override;

    continue_reading_result continue_reading() override;

    void close() override;

    void write(size_t num_bytes, const void* data) override;

 private:

    bool m_dirty;
    policy_flag m_policy;
    size_t m_policy_buffer_size;
    input_stream_ptr m_in;
    intrusive_ptr<broker> m_self;
    cow_tuple<atom_value, uint32_t, util::buffer> m_read;

};

} } // namespace cppa::network

#endif // IO_ACTOR_BACKEND_HPP
