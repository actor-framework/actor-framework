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


#include <string>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"

using std::enable_if;

namespace cppa {

namespace {

constexpr size_t chunk_size = 512;
constexpr size_t ui32_size = sizeof(std::uint32_t);

class binary_writer {

 public:

    binary_writer(util::buffer* sink) : m_sink(sink) { }

    template<typename T>
    static inline void write_int(util::buffer* sink, const T& value) {
        sink->write(sizeof(T), &value, util::grow_if_needed);
    }

    static inline void write_string(util::buffer* sink,
                                    const std::string& str) {
        write_int(sink, static_cast<std::uint32_t>(str.size()));
        sink->write(str.size(), str.c_str(), util::grow_if_needed);
    }

    template<typename T>
    void operator()(const T& value,
                    typename enable_if<std::is_integral<T>::value>::type* = 0) {
        write_int(m_sink, value);
    }

    template<typename T>
    void operator()(const T& value,
                    typename enable_if<std::is_floating_point<T>::value>::type* = 0) {
        // write floating points as strings
        std::string str = std::to_string(value);
        (*this)(str);
    }

    void operator()(const std::string& str) {
        write_string(m_sink, str);
    }

    void operator()(const std::u16string& str) {
        write_int(m_sink, static_cast<std::uint32_t>(str.size()));
        for (char16_t c : str) {
            // force writer to use exactly 16 bit
            write_int(m_sink, static_cast<std::uint16_t>(c));
        }
    }

    void operator()(const std::u32string& str) {
        write_int(m_sink, static_cast<std::uint32_t>(str.size()));
        for (char32_t c : str) {
            // force writer to use exactly 32 bit
            write_int(m_sink, static_cast<std::uint32_t>(c));
        }
    }

 private:

    util::buffer* m_sink;

};

} // namespace <anonymous>

binary_serializer::binary_serializer(util::buffer* buf)
: m_obj_count(0), m_begin_pos(0), m_sink(buf) {
}

void binary_serializer::begin_object(const std::string& tname) {
    if (++m_obj_count == 1) {
        // store a dummy size in the buffer that is
        // eventually updated on matching end_object()
        m_begin_pos = m_sink->size();
        std::uint32_t dummy_size = 0;
        m_sink->write(sizeof(std::uint32_t), &dummy_size, util::grow_if_needed);
    }
    binary_writer::write_string(m_sink, tname);
}

void binary_serializer::end_object() {
    if (--m_obj_count == 0) {
        // update the size in the buffer
        auto data = m_sink->data();
        auto s = static_cast<std::uint32_t>(m_sink->size()
                                            - (m_begin_pos + ui32_size));
        auto wr_pos = data + m_begin_pos;
        memcpy(wr_pos, &s, sizeof(std::uint32_t));
    }
}

void binary_serializer::begin_sequence(size_t list_size) {
    binary_writer::write_int(m_sink, static_cast<std::uint32_t>(list_size));
}

void binary_serializer::end_sequence() { }

void binary_serializer::write_value(const primitive_variant& value) {
    value.apply(binary_writer(m_sink));
}

void binary_serializer::write_raw(size_t num_bytes, const void* data) {
    m_sink->write(num_bytes, data, util::grow_if_needed);
}

void binary_serializer::write_tuple(size_t size,
                                    const primitive_variant* values) {
    const primitive_variant* end = values + size;
    for ( ; values != end; ++values) {
        write_value(*values);
    }
}

void binary_serializer::reset() {
    m_obj_count = 0;
}

} // namespace cppa
