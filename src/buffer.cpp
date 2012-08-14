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


#include <ios>      // std::ios_base::failure
#include <cstring>
#include <utility>

#include "cppa/util/buffer.hpp"
#include "cppa/util/input_stream.hpp"

namespace cppa { namespace util {

namespace {

const size_t default_chunk_size = 512;
const size_t default_max_size   = 16 * 1024 * 1024;

} // namespace anonymous

buffer::buffer()
: m_data(0), m_written(0), m_allocated(0), m_final_size(0)
, m_chunk_size(default_chunk_size), m_max_buffer_size(default_max_size) { }

buffer::buffer(size_t chunk_size, size_t max_buffer_size)
: m_data(0), m_written(0), m_allocated(0), m_final_size(0)
, m_chunk_size(chunk_size), m_max_buffer_size(max_buffer_size) { }

buffer::buffer(buffer&& other)
: m_data(other.m_data), m_written(other.m_written)
, m_allocated(other.m_allocated), m_final_size(other.m_final_size)
, m_chunk_size(other.m_chunk_size), m_max_buffer_size(other.m_max_buffer_size) {
    other.m_data = nullptr;
    other.m_written = other.m_allocated = other.m_final_size = 0;
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

void buffer::reset(size_t new_final_size) {
    if (new_final_size > m_max_buffer_size) {
        m_written = 0;
        m_allocated = 0;
        m_final_size = 0;
        delete[] m_data;
        m_data = nullptr;
        throw std::ios_base::failure("maximum buffer size exceeded");
    }
    m_written = 0;
    m_final_size = new_final_size;
    if (new_final_size > m_allocated) {
        auto remainder = (new_final_size % m_chunk_size);
        if (remainder == 0) {
            m_allocated = new_final_size;
        }
        else {
            m_allocated = (new_final_size - remainder) + m_chunk_size;
        }
        delete[] m_data;
        m_data = new char[m_allocated];
    }
}

void buffer::acquire(size_t num_bytes) {
    if (!m_data) {
        reset(num_bytes);
    }
    else if (remaining() < num_bytes) {
        m_final_size += num_bytes - remaining();
        if ((m_allocated - m_written) < num_bytes) {
            auto remainder = (m_final_size % m_chunk_size);
            if (remainder == 0) {
                m_allocated = m_final_size;
            }
            else {
                m_allocated = (m_final_size - remainder) + m_chunk_size;
            }
            auto old_data = m_data;
            m_data = new char[m_allocated];
            memcpy(m_data, old_data, m_written);
            delete[] old_data;
        }
    }
}

void buffer::erase_leading(size_t num_bytes) {
    if (num_bytes >= size()) {
        clear();
    }
    else {
        memmove(m_data, m_data + num_bytes, size() - num_bytes);
        dec_size(num_bytes);
    }
}

void buffer::erase_trailing(size_t num_bytes) {
    if (num_bytes >= size()) {
        clear();
    }
    else {
        dec_size(num_bytes);
    }
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

void buffer::append_from(input_stream* istream) {
    CPPA_REQUIRE(remaining() > 0);
    auto num_bytes = istream->read_some(wr_ptr(), remaining());
    if (num_bytes > 0) {
        inc_size(num_bytes);
    }
}

} } // namespace cppa::util
