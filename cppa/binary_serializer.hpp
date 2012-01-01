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


#ifndef BINARY_SERIALIZER_HPP
#define BINARY_SERIALIZER_HPP

#include <utility>
#include "cppa/serializer.hpp"

namespace cppa {

namespace detail { class binary_writer; }

class binary_serializer : public serializer
{

    friend class detail::binary_writer;

    char* m_begin;
    char* m_end;
    char* m_wr_pos;

    // make sure that it's safe to write num_bytes bytes to m_wr_pos
    void acquire(size_t num_bytes);

 public:

    binary_serializer();

    ~binary_serializer();

    void begin_object(std::string const& tname);

    void end_object();

    void begin_sequence(size_t list_size);

    void end_sequence();

    void write_value(primitive_variant const& value);

    void write_tuple(size_t size, primitive_variant const* values);

    /**
     * @brief Takes the internal buffer and returns it.
     *
     */
    std::pair<size_t, char*> take_buffer();

    /**
     * @brief Returns the number of written bytes.
     */
    size_t size() const;

    /**
     * @brief Returns a pointer to the internal buffer.
     */
    char const* data() const;

    /**
     * @brief Resets the internal buffer.
     */
    void reset();

};

} // namespace cppa

#endif // BINARY_SERIALIZER_HPP
