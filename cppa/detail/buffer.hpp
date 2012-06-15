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

#include <ios> // std::ios_base::failure
#include <iostream>

#include <string.h>

#include "cppa/detail/native_socket.hpp"

namespace cppa { namespace detail {

template<size_t ChunkSize, size_t MaxBufferSize, typename DataType = char>
class buffer {

    DataType* m_data;
    size_t m_written;
    size_t m_allocated;
    size_t m_final_size;

    template<typename F>
    bool append_impl(F&& fun, bool throw_on_error) {
        auto recv_result = fun();
        if (recv_result == 0) {
            // connection closed
            if (throw_on_error) {
                throw std::ios_base::failure("cannot read from a closed pipe/socket");
            }
            return false;
        }
        else if (recv_result < 0) {
            switch (errno) {
                case EAGAIN: {
                    // rdflags or sfd is set to non-blocking,
                    // this is not treated as error
                    return true;
                }
                default: {
                    // a "real" error occured;
                    if (throw_on_error) {
                        char* cstr = strerror(errno);
                        std::string errmsg = cstr;
                        free(cstr);
                        throw std::ios_base::failure(std::move(errmsg));
                    }
                    return false;
                }
            }
        }
        inc_written(static_cast<size_t>(recv_result));
        return true;
    }

 public:

    buffer() : m_data(nullptr), m_written(0), m_allocated(0), m_final_size(0) {
    }

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
        m_written = 0;
        m_final_size = new_final_size;
        if (new_final_size > m_allocated) {
            if (new_final_size > MaxBufferSize) {
                throw std::ios_base::failure("maximum buffer size exceeded");
            }
            auto remainder = (new_final_size % ChunkSize);
            if (remainder == 0) {
                m_allocated = new_final_size;
            }
            else {
                m_allocated = (new_final_size - remainder) + ChunkSize;
            }
            delete[] m_data;
            m_data = new DataType[m_allocated];
        }
    }

    bool ready() {
        return m_written == m_final_size;
    }

    // pointer to the current write position
    DataType* wr_ptr() {
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

    DataType* data() {
        return m_data;
    }

    inline bool full() {
        return remaining() == 0;
    }

    bool append_from_file_descriptor(int fd, bool throw_on_error = false) {
        auto fun = [=]() -> int {
            return ::read(fd, this->wr_ptr(), this->remaining());
        };
        return append_impl(fun, throw_on_error);
    }

    bool append_from(native_socket_type sfd,
                     int rdflags = 0,
                     bool throw_on_error = false) {
        auto fun = [=]() -> int {
            return ::recv(sfd, this->wr_ptr(), this->remaining(), rdflags);
        };
        return append_impl(fun, throw_on_error);
    }

};

} } // namespace cppa::detail

#endif // CPPA_BUFFER_HPP
