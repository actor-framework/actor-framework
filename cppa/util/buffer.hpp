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

#include <cstddef> // size_t

namespace cppa { namespace util {

class input_stream;

enum buffer_write_policy {
    grow_if_needed,
    do_not_grow
};

/**
 *
 */
class buffer {

 public:

    buffer();

    buffer(size_t chunk_size, size_t max_buffer_size);

    buffer(buffer&& other);

    buffer& operator=(buffer&& other);

    ~buffer();

    /**
     * @brief Clears the buffer's content.
     */
    inline void clear() {
        m_written = 0;
    }

    /**
     * @brief Clears the buffer's content and sets the new final size to
     *        @p new_final_size.
     */
    void reset(size_t new_final_size = 0);

    /**
     * @brief Makes sure the buffer can at least write @p num_bytes additional
     *        bytes and increases the final size if needed.
     */
    void acquire(size_t num_bytes);

    void erase_leading(size_t num_bytes);

    void erase_trailing(size_t num_bytes);

    inline size_t size() const {
        return m_written;
    }

    inline size_t final_size() const {
        return m_final_size;
    }

    inline size_t remaining() const {
        return m_final_size - m_written;
    }

    inline const char* data() const {
        return m_data;
    }

    inline char* data() {
        return m_data;
    }

    inline bool full() const {
        return remaining() == 0;
    }

    inline bool empty() const {
        return size() == 0;
    }

    inline size_t maximum_size() const {
        return m_max_buffer_size;
    }

    void write(size_t num_bytes, const void* data, buffer_write_policy wp);

    /**
     * @brief Appends up to @p remaining() bytes from @p istream to the buffer.
     */
    void append_from(input_stream* istream);

 private:

    // pointer to the current write position
    inline char* wr_ptr() {
        return m_data + m_written;
    }

    inline void inc_size(size_t value) {
        m_written += value;
    }

    inline void dec_size(size_t value) {
        m_written -= value;
    }

    char*  m_data;
    size_t m_written;
    size_t m_allocated;
    size_t m_final_size;
    size_t m_chunk_size;
    size_t m_max_buffer_size;

};

} } // namespace cppa::detail

#endif // CPPA_BUFFER_HPP
