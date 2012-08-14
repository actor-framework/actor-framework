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


#ifndef CPPA_BUFFER_HPP
#define CPPA_BUFFER_HPP

#include <ios>      // std::ios_base::failure
#include <cstring>
#include <iostream>

#include "cppa/util/input_stream.hpp"

namespace cppa { namespace detail {

template<size_t ChunkSize, size_t MaxBufferSize>
class buffer {

    char* m_data;
    size_t m_written;
    size_t m_allocated;
    size_t m_final_size;

 public:

    buffer() : m_data(nullptr), m_written(0), m_allocated(0), m_final_size(0) {}

    buffer(buffer&& other)
    : m_data(other.m_data), m_written(other.m_written)
    , m_allocated(other.m_allocated), m_final_size(other.m_final_size) {
        other.m_data = nullptr;
        other.m_written = other.m_allocated = other.m_final_size = 0;
    }

    ~buffer() {
        delete[] m_data;
    }

    void clear() {
        m_written = 0;
    }

    void reset(size_t new_final_size = 0) {
        if (new_final_size > MaxBufferSize) {
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
            auto remainder = (new_final_size % ChunkSize);
            if (remainder == 0) {
                m_allocated = new_final_size;
            }
            else {
                m_allocated = (new_final_size - remainder) + ChunkSize;
            }
            delete[] m_data;
            m_data = new char[m_allocated];
        }
    }

    // pointer to the current write position
    char* wr_ptr() {
        return m_data + m_written;
    }

    size_t size() {
        return m_written;
    }

    size_t final_size() {
        return m_final_size;
    }

    size_t remaining() {
        return m_final_size - m_written;
    }

    void inc_written(size_t value) {
        m_written += value;
    }

    char* data() {
        return m_data;
    }

    inline bool full() {
        return remaining() == 0;
    }

    void append_from(util::input_stream* istream) {
        CPPA_REQUIRE(remaining() > 0);
        auto num_bytes = istream->read_some(wr_ptr(), remaining());
        if (num_bytes > 0) {
            inc_written(num_bytes);
        }
    }

};

} } // namespace cppa::detail

#endif // CPPA_BUFFER_HPP
