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


#ifndef IO_SERVICE_HPP
#define IO_SERVICE_HPP

#include <cstddef>

namespace cppa { namespace io {

class io_handle {

 public:

    virtual ~io_handle();

    /**
     * @brief Denotes when an actor will receive a read buffer.
     */
    enum policy_flag { at_least, at_most, exactly };

    /**
     * @brief Closes the network connection.
     */
    virtual void close() = 0;

    /**
     * @brief Asynchronously sends @p size bytes of @p data.
     */
    virtual void write(size_t size, const void* data) = 0;

    /**
     * @brief Adjusts the rule receiving 'IO_receive' messages.
     *        The default settings are <tt>policy = io_handle::at_least</tt>
     *        and <tt>buffer_size = 0</tt>.
     */
    virtual void receive_policy(policy_flag policy, size_t buffer_size) = 0;

};

} } // namespace cppa::network

#endif // IO_SERVICE_HPP
