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


#ifndef BUFFERED_WRITER_HPP
#define BUFFERED_WRITER_HPP

#include "cppa/util/buffer.hpp"

#include "cppa/network/output_stream.hpp"
#include "cppa/network/continuable_io.hpp"

namespace cppa { namespace network {

class middleman;

class buffered_writer : public continuable_io {

    typedef continuable_io super;

 public:

    buffered_writer(middleman* parent,
                    native_socket_type read_fd,
                    output_stream_ptr out);

    continue_writing_result continue_writing() override;

    inline bool has_unwritten_data() const {
        return m_has_unwritten_data;
    }

 protected:

    void write(size_t num_bytes, const void* data);

    void register_for_writing();

    inline util::buffer& write_buffer() {
        return m_buf;
    }

 private:

    middleman* m_middleman;
    output_stream_ptr m_out;
    bool m_has_unwritten_data;
    util::buffer m_buf;

};

} } // namespace cppa::network

#endif // BUFFERED_WRITER_HPP
