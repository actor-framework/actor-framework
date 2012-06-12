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

namespace {

constexpr size_t chunk_size = 512;
constexpr size_t ui32_size = sizeof(std::uint32_t);

} // namespace <anonymous>

namespace cppa {

namespace detail {

class binary_writer {

    binary_serializer* m_serializer;

 public:

    binary_writer(binary_serializer* s) : m_serializer(s) { }

    template<typename T>
    inline static void write_int(binary_serializer* bs, const T& value) {
        memcpy(bs->m_wr_pos, &value, sizeof(T));
        bs->m_wr_pos += sizeof(T);
    }

    inline static void write_string(binary_serializer* bs,
                                    const std::string& str) {
        write_int(bs, static_cast<std::uint32_t>(str.size()));
        memcpy(bs->m_wr_pos, str.c_str(), str.size());
        bs->m_wr_pos += str.size();
    }

    template<typename T>
    void operator()(const T& value,
                    typename enable_if<std::is_integral<T>::value>::type* = 0) {
        m_serializer->acquire(sizeof(T));
        write_int(m_serializer, value);
    }

    template<typename T>
    void operator()(const T&,
                    typename enable_if<std::is_floating_point<T>::value>::type* = 0) {
        throw std::logic_error("binary_serializer::write_floating_point "
                               "not implemented yet");
    }

    void operator()(const std::string& str) {
        m_serializer->acquire(sizeof(std::uint32_t) + str.size());
        write_string(m_serializer, str);
    }

    void operator()(const std::u16string& str) {
        m_serializer->acquire(sizeof(std::uint32_t) + str.size());
        write_int(m_serializer, static_cast<std::uint32_t>(str.size()));
        for (char16_t c : str) {
            // force writer to use exactly 16 bit
            write_int(m_serializer, static_cast<std::uint16_t>(c));
        }
    }

    void operator()(const std::u32string& str) {
        m_serializer->acquire(sizeof(std::uint32_t) + str.size());
        write_int(m_serializer, static_cast<std::uint32_t>(str.size()));
        for (char32_t c : str) {
            // force writer to use exactly 32 bit
            write_int(m_serializer, static_cast<std::uint32_t>(c));
        }
    }

};

} // namespace detail

binary_serializer::binary_serializer() : m_begin(0), m_end(0), m_wr_pos(0) {
}

binary_serializer::~binary_serializer() {
    delete[] m_begin;
}

void binary_serializer::acquire(size_t num_bytes) {
    if (!m_begin) {
        num_bytes += ui32_size;
        size_t new_size = chunk_size;
        while (new_size <= num_bytes) {
            new_size += chunk_size;
        }
        m_begin = new char[new_size];
        m_end = m_begin + new_size;
        m_wr_pos = m_begin + ui32_size;
    }
    else {
        char* next_wr_pos = m_wr_pos + num_bytes;
        if (next_wr_pos > m_end) {
            size_t new_size =   static_cast<size_t>(m_end - m_begin)
                              + chunk_size;
            while ((m_begin + new_size) < next_wr_pos) {
                new_size += chunk_size;
            }
            char* begin = new char[new_size];
            auto used_bytes = static_cast<size_t>(m_wr_pos - m_begin);
            if (used_bytes > 0) {
                memcpy(m_begin, begin, used_bytes);
            }
            delete[] m_begin;
            m_begin = begin;
            m_end = m_begin + new_size;
            m_wr_pos = m_begin + used_bytes;
        }
    }
}

void binary_serializer::begin_object(const std::string& tname) {
    acquire(sizeof(std::uint32_t) + tname.size());
    detail::binary_writer::write_string(this, tname);
}

void binary_serializer::end_object() {
}

void binary_serializer::begin_sequence(size_t list_size) {
    acquire(sizeof(std::uint32_t));
    detail::binary_writer::write_int(this,
                                     static_cast<std::uint32_t>(list_size));
}

void binary_serializer::end_sequence() {
}

void binary_serializer::write_value(const primitive_variant& value) {
    value.apply(detail::binary_writer(this));
}

void binary_serializer::write_tuple(size_t size,
                                    const primitive_variant* values) {
    const primitive_variant* end = values + size;
    for ( ; values != end; ++values) {
        write_value(*values);
    }
}

size_t binary_serializer::sendable_size() const {
    return static_cast<size_t>(m_wr_pos - m_begin);
}

size_t binary_serializer::size() const {
    return sendable_size() - ui32_size;
}

const char* binary_serializer::data() const {
    return m_begin + ui32_size;
}

const char* binary_serializer::sendable_data() {
    auto s = static_cast<std::uint32_t>(size());
    memcpy(m_begin, &s, ui32_size);
    return m_begin;
}

void binary_serializer::reset() {
    m_wr_pos = m_begin + ui32_size;
}

} // namespace cppa
