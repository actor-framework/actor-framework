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


#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iterator>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <type_traits>

#include "cppa/self.hpp"
#include "cppa/logging.hpp"
#include "cppa/type_lookup_table.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/detail/ieee_754.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

using namespace std;

namespace cppa {

namespace {

typedef const void* pointer;

const char* as_char_pointer(pointer ptr) {
    return reinterpret_cast<const char*>(ptr);
}

pointer advanced(pointer ptr, size_t num_bytes) {
    return reinterpret_cast<const char*>(ptr) + num_bytes;
}

inline void range_check(pointer begin, pointer end, size_t read_size) {
    if (advanced(begin, read_size) > end) {
        CPPA_LOGF(CPPA_ERROR, self, "range_check failed");
        throw out_of_range("binary_deserializer::read_range()");
    }
}

pointer read_range(pointer begin, pointer end, string& storage);

template<typename T>
pointer read_range(pointer begin, pointer end, T& storage,
                   typename enable_if<is_integral<T>::value>::type* = 0) {
    range_check(begin, end, sizeof(T));
    memcpy(&storage, begin, sizeof(T));
    return advanced(begin, sizeof(T));
}

template<typename T>
pointer read_range(pointer begin, pointer end, T& storage,
                   typename enable_if<is_floating_point<T>::value>::type* = 0) {
    typename detail::ieee_754_trait<T>::packed_type tmp;
    auto result = read_range(begin, end, tmp);
    storage = detail::unpack754(tmp);
    return result;
}

// the IEEE-754 conversion does not work for long double
// => fall back to string serialization (event though it sucks)
pointer read_range(pointer begin, pointer end, long double& storage) {
    std::string tmp;
    auto result = read_range(begin, end, tmp);
    std::istringstream iss{std::move(tmp)};
    iss >> storage;
    return result;
}

pointer read_range(pointer begin, pointer end, string& storage) {
    uint32_t str_size;
    begin = read_range(begin, end, str_size);
    range_check(begin, end, str_size);
    storage.clear();
    storage.reserve(str_size);
    pointer cpy_end = advanced(begin, str_size);
    copy(as_char_pointer(begin), as_char_pointer(cpy_end), back_inserter(storage));
    return advanced(begin, str_size);
}

template<typename CharType, typename StringType>
pointer read_unicode_string(pointer begin, pointer end, StringType& str) {
    uint32_t str_size;
    begin = read_range(begin, end, str_size);
    str.reserve(str_size);
    for (size_t i = 0; i < str_size; ++i) {
        CharType c;
        begin = read_range(begin, end, c);
        str += static_cast<typename StringType::value_type>(c);
    }
    return begin;
}

pointer read_range(pointer begin, pointer end, atom_value& storage) {
    std::uint64_t tmp;
    auto result = read_range(begin, end, tmp);
    storage = static_cast<atom_value>(tmp);
    return result;
}

pointer read_range(pointer begin, pointer end, u16string& storage) {
    // char16_t is guaranteed to has *at least* 16 bytes,
    // but not to have *exactly* 16 bytes; thus use uint16_t
    return read_unicode_string<uint16_t>(begin, end, storage);
}

pointer read_range(pointer begin, pointer end, u32string& storage) {
    // char32_t is guaranteed to has *at least* 32 bytes,
    // but not to have *exactly* 32 bytes; thus use uint32_t
    return read_unicode_string<uint32_t>(begin, end, storage);
}

struct pt_reader {

    pointer begin;
    pointer end;

    pt_reader(pointer bbegin, pointer bend) : begin(bbegin), end(bend) { }

    template<typename T>
    inline void operator()(T& value) {
        begin = read_range(begin, end, value);
    }

};

} // namespace <anonmyous>

binary_deserializer::binary_deserializer(const void* buf, size_t buf_size,
                                         actor_addressing* addressing,
                                         type_lookup_table* tbl)
: super(addressing, tbl), m_pos(buf), m_end(advanced(buf, buf_size)) { }

binary_deserializer::binary_deserializer(const void* bbegin, const void* bend,
                                         actor_addressing* addressing,
                                         type_lookup_table* tbl)
: super(addressing, tbl), m_pos(bbegin), m_end(bend) { }

const uniform_type_info* binary_deserializer::begin_object() {
    std::uint8_t flag;
    m_pos = read_range(m_pos, m_end, flag);
    if (flag == 1) {
        string tname;
        m_pos = read_range(m_pos, m_end, tname);
        auto uti = get_uniform_type_info_map()->by_uniform_name(tname);
        if (!uti) {
            std::string err = "received type name \"";
            err += tname;
            err += "\" but no such type is known";
            throw std::runtime_error(err);
        }
        return uti;
    }
    else {
        std::uint32_t type_id;
        m_pos = read_range(m_pos, m_end, type_id);
        auto it = incoming_types();
        if (!it) {
            std::string err = "received type ID ";
            err += std::to_string(type_id);
            err += " but incoming_types() == nullptr";
            throw std::runtime_error(err);
        }
        auto uti = it->by_id(type_id);
        if (!uti) throw std::runtime_error("received unknown type id");
        return uti;
    }
}

void binary_deserializer::end_object() { }

size_t binary_deserializer::begin_sequence() {
    CPPA_LOGMF(CPPA_TRACE, self, "");
    static_assert(sizeof(size_t) >= sizeof(uint32_t),
                  "sizeof(size_t) < sizeof(uint32_t)");
    uint32_t result;
    m_pos = read_range(m_pos, m_end, result);
    return static_cast<size_t>(result);
}

void binary_deserializer::end_sequence() { }

primitive_variant binary_deserializer::read_value(primitive_type ptype) {
    primitive_variant val(ptype);
    pt_reader ptr(m_pos, m_end);
    val.apply(ptr);
    m_pos = ptr.begin;
    return val;
}

void binary_deserializer::read_tuple(size_t size,
                                     const primitive_type* ptypes,
                                     primitive_variant* storage) {
    for (auto end = ptypes + size; ptypes != end; ++ptypes) {
        *storage = move(read_value(*ptypes));
        ++storage;
    }
}

void binary_deserializer::read_raw(size_t num_bytes, void* storage) {
    range_check(m_pos, m_end, num_bytes);
    memcpy(storage, m_pos, num_bytes);
    m_pos = advanced(m_pos, num_bytes);
}

} // namespace cppa
