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


#ifndef CONTINUABLE_WRITER_HPP
#define CONTINUABLE_WRITER_HPP

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/network/continuable_reader.hpp"

namespace cppa { namespace network {

/**
 * @brief Denotes the return value of
 *        {@link continuable_io::continue_writing()}.
 */
enum continue_writing_result {
    write_failure,
    write_closed,
    write_continue_later,
    write_done
};

/**
 * @brief An object performing asynchronous input and output.
 */
class continuable_io : public continuable_reader {

    typedef continuable_reader super;

 public:

    /**
     * @brief Returns the file descriptor for outgoing data.
     */
    native_socket_type write_handle() const {
        return m_wr;
    }

    continuable_io* as_io();

    /**
     * @brief Writes to {@link write_handle()}.
     */
    virtual continue_writing_result continue_writing() = 0;

 protected:

    continuable_io(native_socket_type read_fd, native_socket_type write_fd);

 private:

    native_socket_type m_wr;

};

typedef intrusive_ptr<continuable_io> continuable_io_ptr;

} } // namespace cppa::network

#endif // CONTINUABLE_WRITER_HPP
