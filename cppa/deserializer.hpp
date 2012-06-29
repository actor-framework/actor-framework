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


#ifndef CPPA_DESERIALIZER_HPP
#define CPPA_DESERIALIZER_HPP

#include <string>
#include <cstddef>

#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"

namespace cppa {

class object;

/**
 * @ingroup TypeSystem
 * @brief Technology-independent deserialization interface.
 */
class deserializer {

    deserializer(const deserializer&) = delete;
    deserializer& operator=(const deserializer&) = delete;

 public:

    deserializer() = default;

    virtual ~deserializer();

    /**
     * @brief Seeks the beginning of the next object and return
     *        its uniform type name.
     */
    virtual std::string seek_object() = 0;

    /**
     * @brief Equal to {@link seek_object()} but doesn't
     *        modify the internal in-stream position.
     */
    virtual std::string peek_object() = 0;

    /**
     * @brief Begins deserialization of an object of type @p type_name.
     * @param type_name The platform-independent @p libcppa type name.
     */
    virtual void begin_object(const std::string& type_name) = 0;

    /**
     * @brief Ends deserialization of an object.
     */
    virtual void end_object() = 0;

    /**
     * @brief Begins deserialization of a sequence.
     * @returns The size of the sequence.
     */
    virtual size_t begin_sequence() = 0;

    /**
     * @brief Ends deserialization of a sequence.
     */
    virtual void end_sequence() = 0;

    /**
     * @brief Reads a primitive value from the data source of type @p ptype.
     * @param ptype Expected primitive data type.
     * @returns A primitive value of type @p ptype.
     */
    virtual primitive_variant read_value(primitive_type ptype) = 0;

    /**
     * @brief Reads a tuple of primitive values from the data
     *        source of the types @p ptypes.
     * @param num The size of the tuple.
     * @param ptypes Array of expected primitive data types.
     * @param storage Array of size @p num, storing the result of this function.
     */
    virtual void read_tuple(size_t num,
                            const primitive_type* ptypes,
                            primitive_variant* storage   ) = 0;

};

/**
 * @brief Deserializes a value and stores the result in @p storage.
 * @param d A valid deserializer.
 * @param storage An that should contain the deserialized value.
 * @returns @p d
 * @relates deserializer
 */
deserializer& operator>>(deserializer& d, object& storage);

} // namespace cppa

#endif // CPPA_DESERIALIZER_HPP
