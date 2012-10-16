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


#ifndef CONTINUABLE_READER_HPP
#define CONTINUABLE_READER_HPP

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"

#include "cppa/network/protocol.hpp"

namespace cppa { namespace network {

class middleman;

enum continue_reading_result {
    read_failure,
    read_closed,
    read_continue_later
};

class continuable_writer;

class continuable_reader : virtual public ref_counted {

 public:

    /**
     * @brief Returns the file descriptor for incoming data.
     */
    inline native_socket_type read_handle() const { return m_rd; }

    /**
     * @brief Reads from {@link read_handle()}.
     */
    virtual continue_reading_result continue_reading() = 0;

    /**
     * @return Casts @p this to a continuable_writer or returns @p nullptr.
     */
    virtual continuable_writer* as_writer();

 protected:

    continuable_reader(native_socket_type rd);

 private:

    native_socket_type m_rd;

};

typedef intrusive_ptr<continuable_reader> continuable_reader_ptr;

} } // namespace cppa::network

#endif // CONTINUABLE_READER_HPP
