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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#include <atomic>
#include <ios>      // std::ios_base::failure
#include <cstring>
#include <utility>

#include "cppa/cppa.hpp"
#include "cppa/util/buffer.hpp"
#include "cppa/io/input_stream.hpp"

namespace cppa {
namespace util {

namespace {

const size_t default_chunk_size = 512;

} // namespace anonymous

buffer::buffer()
: m_data(nullptr), m_written(0), m_allocated(0), m_final_size(0)
, m_chunk_size(default_chunk_size), m_max_buffer_size(max_msg_size()) { }

buffer::buffer(size_t chunk_size, size_t max_buffer_size)
: m_data(nullptr), m_written(0), m_allocated(0), m_final_size(0)
, m_chunk_size(chunk_size), m_max_buffer_size(max_buffer_size) { }

buffer::buffer(buffer&& other)
: m_data(other.m_data), m_written(other.m_written)
, m_allocated(other.m_allocated), m_final_size(other.m_final_size)
, m_chunk_size(other.m_chunk_size), m_max_buffer_size(other.m_max_buffer_size) {
    other.m_data = nullptr;
    other.m_written = other.m_allocated = other.m_final_size = 0;
}

buffer::buffer(const buffer& other)
: m_data(nullptr), m_written(0), m_allocated(0), m_final_size(0)
, m_chunk_size(default_chunk_size), m_max_buffer_size(max_msg_size()) {
    write(other, grow_if_needed);
}

buffer& buffer::operator=(const buffer& other) {
    clear();
    write(other, grow_if_needed);
    return *this;
}


buffer& buffer::operator=(buffer&& other) {
    std::swap(m_data,            other.m_data);
    std::swap(m_written,         other.m_written);
    std::swap(m_allocated,       other.m_allocated);
    std::swap(m_final_size,      other.m_final_size);
    std::swap(m_chunk_size,      other.m_chunk_size);
    std::swap(m_max_buffer_size, other.m_max_buffer_size);
    return *this;
}

buffer::~buffer() {
    delete[] m_data;
}

void buffer::final_size(size_t new_value) {
    if (new_value > m_max_buffer_size) {
        throw std::invalid_argument("new_value > maximum_size()");
    }
    m_final_size = new_value;


    if (new_value > m_allocated) {
        auto remainder = (new_value % m_chunk_size);
        if (remainder == 0) m_allocated = new_value;
        else m_allocated = (new_value - remainder) + m_chunk_size;

        if (remainder == 0) m_allocated = new_value;
        else m_allocated = (new_value - remainder) + m_chunk_size;



        delete[] m_data;
        m_data = new char[m_allocated];
    }
}

void buffer::acquire(size_t num_bytes) {
    if (m_data == nullptr) {
        if (num_bytes > m_final_size) m_final_size = num_bytes;
        m_allocated = adjust(m_final_size);
        m_data = new char[m_allocated];
    }
    if (remaining() >= num_bytes) return; // nothing to do
    m_final_size += num_bytes - remaining();
    if ((m_allocated - m_written) < num_bytes) {
        auto old_data = m_data;
        m_allocated = adjust(m_final_size);
        m_data = new char[m_allocated];
        memcpy(m_data, old_data, m_written);
        delete[] old_data;
    }
}

void buffer::erase_leading(size_t num_bytes) {
    if (num_bytes >= size()) clear();
    else {
        memmove(m_data, m_data + num_bytes, size() - num_bytes);
        dec_size(num_bytes);
    }
}

void buffer::erase_trailing(size_t num_bytes) {
    if (num_bytes >= size()) clear();
    else dec_size(num_bytes);
}

void buffer::write(size_t num_bytes, const void* data, buffer_write_policy wp) {
    if (wp == grow_if_needed) {
        acquire(num_bytes);
    }
    else if (num_bytes > remaining()) {
        throw std::ios_base::failure("final buffer size exceeded");
    }
    memcpy(wr_ptr(), data, num_bytes);
    inc_size(num_bytes);
}

void buffer::write(const buffer& other, buffer_write_policy wp) {
    write(other.size(), other.data(), wp);
}

void buffer::write(buffer&& other, buffer_write_policy wp) {
    if (empty() && (wp == grow_if_needed || other.size() <= remaining())) {
        *this = std::move(other);
    }
    else write(other.size(), other.data(), wp);
    other.clear();
}

void buffer::append_from(io::input_stream* istream) {
    CPPA_REQUIRE(remaining() > 0);
    auto num_bytes = istream->read_some(wr_ptr(), remaining());
    if (num_bytes > 0) {
        inc_size(num_bytes);
    }
}

} // namespace util
} // namespace cppa

