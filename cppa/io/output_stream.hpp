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


#ifndef CPPA_IO_OUTPUT_STREAM_HPP
#define CPPA_IO_OUTPUT_STREAM_HPP

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {
namespace io {

/**
 * @brief An abstract output stream interface.
 */
class output_stream : public virtual ref_counted {

 public:

    ~output_stream();

    /**
     * @brief Returns the internal file descriptor. This descriptor is needed
     *        for socket multiplexing using select().
     */
    virtual native_socket_type write_handle() const = 0;

    /**
     * @brief Writes @p num_bytes bytes from @p buf to the data sink.
     * @note This member function blocks until @p num_bytes were successfully
     *       written.
     * @throws std::ios_base::failure
     */
    virtual void write(const void* buf, size_t num_bytes) = 0;

    /**
     * @brief Tries to write up to @p num_bytes bytes from @p buf.
     * @returns The number of written bytes.
     * @throws std::ios_base::failure
     */
    virtual size_t write_some(const void* buf, size_t num_bytes) = 0;

};

/**
 * @brief An output stream pointer.
 */
typedef intrusive_ptr<output_stream> output_stream_ptr;

} // namespace io
} // namespace cppa

#endif // CPPA_IO_OUTPUT_STREAM_HPP
