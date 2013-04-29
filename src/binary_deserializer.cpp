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
#include "cppa/binary_deserializer.hpp"

using namespace std;

namespace cppa {

namespace {

typedef const char* iterator;

inline void range_check(iterator begin, iterator end, size_t read_size) {
    if ((begin + read_size) > end) {
        CPPA_LOGF(CPPA_ERROR, self, "range_check failed");
        throw out_of_range("binary_deserializer::read_range()");
    }
}

template<typename T>
iterator read_range(iterator begin, iterator end, T& storage,
              typename enable_if<is_integral<T>::value>::type* = 0) {
    range_check(begin, end, sizeof(T));
    memcpy(&storage, begin, sizeof(T));
    return begin + sizeof(T);
}

iterator read_range(iterator begin, iterator end, string& storage) {
    uint32_t str_size;
    begin = read_range(begin, end, str_size);
    range_check(begin, end, str_size);
    storage.clear();
    storage.reserve(str_size);
    iterator cpy_end = begin + str_size;
    copy(begin, cpy_end, back_inserter(storage));
    return begin + str_size;
}

template<typename CharType, typename StringType>
iterator read_unicode_string(iterator begin, iterator end, StringType& str) {
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

// @returns the next iterator position
template<typename T>
iterator read_range(iterator begin, iterator end, T& value,
              typename enable_if<is_floating_point<T>::value>::type* = 0) {
    // floating points are written as strings
    string str;
    auto result = read_unicode_string<char>(begin, end, str);
    istringstream iss(str);
    iss >> value;
    return result;
}

iterator read_range(iterator begin, iterator end, u16string& storage) {
    // char16_t is guaranteed to has *at least* 16 bytes,
    // but not to have *exactly* 16 bytes; thus use uint16_t
    return read_unicode_string<uint16_t>(begin, end, storage);
}

iterator read_range(iterator begin, iterator end, u32string& storage) {
    // char32_t is guaranteed to has *at least* 32 bytes,
    // but not to have *exactly* 32 bytes; thus use uint32_t
    return read_unicode_string<uint32_t>(begin, end, storage);
}

struct pt_reader {

    iterator begin;
    iterator end;

    pt_reader(iterator bbegin, iterator bend) : begin(bbegin), end(bend) { }

    template<typename T>
    inline void operator()(T& value) {
        begin = read_range(begin, end, value);
    }

};

} // namespace <anonmyous>

binary_deserializer::binary_deserializer(const char* buf, size_t buf_size,
                                         actor_addressing* addressing)
: super(addressing), pos(buf), end(buf + buf_size) { }

binary_deserializer::binary_deserializer(const char* bbegin, const char* bend,
                                         actor_addressing* addressing)
: super(addressing), pos(bbegin), end(bend) { }

string binary_deserializer::seek_object() {
    string result;
    pos = read_range(pos, end, result);
    return result;
}

string binary_deserializer::peek_object() {
    string result;
    read_range(pos, end, result);
    return result;
}

void binary_deserializer::begin_object(const string&) { }

void binary_deserializer::end_object() { }

size_t binary_deserializer::begin_sequence() {
    CPPA_LOGMF(CPPA_TRACE, self, "");
    static_assert(sizeof(size_t) >= sizeof(uint32_t),
                  "sizeof(size_t) < sizeof(uint32_t)");
    uint32_t result;
    pos = read_range(pos, end, result);
    return static_cast<size_t>(result);
}

void binary_deserializer::end_sequence() { }

primitive_variant binary_deserializer::read_value(primitive_type ptype) {
    primitive_variant val(ptype);
    pt_reader ptr(pos, end);
    val.apply(ptr);
    pos = ptr.begin;
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
    range_check(pos, end, num_bytes);
    memcpy(storage, pos, num_bytes);
    pos += num_bytes;
}

} // namespace cppa
