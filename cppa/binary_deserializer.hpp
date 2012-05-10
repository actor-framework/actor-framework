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


#ifndef BINARY_DESERIALIZER_HPP
#define BINARY_DESERIALIZER_HPP

#include "cppa/deserializer.hpp"

namespace cppa {

/**
 * @brief Implements the deserializer interface with
 *        a binary serialization protocol.
 */
class binary_deserializer : public deserializer {

    char const* pos;
    char const* end;

    void range_check(size_t read_size);

 public:

    binary_deserializer(char const* buf, size_t buf_size);
    binary_deserializer(char const* begin, char const* end);

    std::string seek_object();
    std::string peek_object();
    void begin_object(std::string const& type_name);
    void end_object();
    size_t begin_sequence();
    void end_sequence();
    primitive_variant read_value(primitive_type ptype);
    void read_tuple(size_t size,
                    primitive_type const* ptypes,
                    primitive_variant* storage);

};

} // namespace cppa

#endif // BINARY_DESERIALIZER_HPP
