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


#ifndef CPPA_SERIALIZER_HPP
#define CPPA_SERIALIZER_HPP

#include <string>
#include <cstddef> // size_t

#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

class actor_addressing;
class primitive_variant;
class type_lookup_table;

/**
 * @ingroup TypeSystem
 * @brief Technology-independent serialization interface.
 */
class serializer {

    serializer(const serializer&) = delete;
    serializer& operator=(const serializer&) = delete;

 public:

    /**
     * @note @p addressing must be guaranteed to outlive the serializer
     */
    serializer(actor_addressing* addressing = nullptr,
               type_lookup_table* outgoing_types = nullptr);

    virtual ~serializer();

    /**
     * @brief Begins serialization of an object of the type
     *        named @p type_name.
     * @param type_name The platform-independent @p libcppa type name.
     */
    virtual void begin_object(const uniform_type_info*) = 0;

    /**
     * @brief Ends serialization of an object.
     */
    virtual void end_object() = 0;

    /**
     * @brief Begins serialization of a sequence of size @p num.
     */
    virtual void begin_sequence(size_t num) = 0;

    /**
     * @brief Ends serialization of a sequence.
     */
    virtual void end_sequence() = 0;

    /**
     * @brief Writes a single value to the data sink.
     * @param value A primitive data value.
     */
    virtual void write_value(const primitive_variant& value) = 0;

    /**
     * @brief Writes a raw block of data.
     * @param num_bytes The size of @p data in bytes.
     * @param data Raw data.
     */
    virtual void write_raw(size_t num_bytes, const void* data) = 0;

    /**
     * @brief Writes @p num values as a tuple to the data sink.
     * @param num Size of the array @p values.
     * @param values An array of size @p num of primitive data values.
     */
    virtual void write_tuple(size_t num, const primitive_variant* values) = 0;

    inline actor_addressing* addressing() {
        return m_addressing;
    }

    inline type_lookup_table* outgoing_types() {
        return m_outgoing_types;
    }

 private:

    actor_addressing* m_addressing;
    type_lookup_table* m_outgoing_types;

};

/**
 * @brief Serializes a value to @p s.
 * @param s A valid serializer.
 * @param what A value of an announced or primitive type.
 * @returns @p s
 * @relates serializer
 */
template<typename T>
serializer& operator<<(serializer& s, const T& what) {
    auto mtype = uniform_typeid<T>();
    if (mtype == nullptr) {
        throw std::logic_error(  "no uniform type info found for "
                               + cppa::detail::to_uniform_name(typeid(T)));
    }
    mtype->serialize(&what, &s);
    return s;
}

} // namespace cppa

#endif // CPPA_SERIALIZER_HPP
