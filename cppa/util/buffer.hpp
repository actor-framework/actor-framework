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


#ifndef CPPA_UTIL_BUFFER_HPP
#define CPPA_UTIL_BUFFER_HPP

#include <cstddef> // size_t

namespace cppa { class deserializer; }

namespace cppa { namespace io { class input_stream; } }

namespace cppa {
namespace util {

class input_stream;

enum buffer_write_policy {
    grow_if_needed,
    do_not_grow
};

/**
 * @brief A buffer implementation with configurable final size
 *        that also supports dynamic growing if needed.
 */
class buffer {

    friend class cppa::deserializer;

 public:

    /**
     * @brief Creates an empty buffer.
     */
    buffer();

    /**
     * @brief Creates a buffer holding at most @p max_buffer_size bytes
     *        that allocates memory in steps of @p chunk_size bytes.
     * @note The default chunk_size used by other constuctors is 512 bytes.
     * @note The default maximum buffer size is 16MB.
     */
    buffer(size_t chunk_size, size_t max_buffer_size);

    buffer(buffer&& other);

    buffer(const buffer& other);

    buffer& operator=(buffer&& other);

    buffer& operator=(const buffer& other);

    ~buffer();

    /**
     * @brief Makes sure the buffer can at least write @p num_bytes additional
     *        bytes, increasing the final size if needed.
     */
    void acquire(size_t num_bytes);

    /**
     * @brief Erases the first @p num_bytes bytes from the buffer.
     */
    void erase_leading(size_t num_bytes);

    /**
     * @brief Erases the last @p num_bytes bytes from the buffer.
     */
    void erase_trailing(size_t num_bytes);

    /**
     * @brief Clears the buffer's content.
     */
    inline void clear();

    /**
     * @brief Returns the size of the buffer's content in bytes.
     */
    inline size_t size() const;

    /**
     * @brief Returns the configured final size of this buffer. This value
     *        can be changed by using {@link final_size()} and controls how much
     *        bytes are consumed when using {@link append_from()}.
     */
    inline size_t final_size() const;

    /**
     * @brief Sets the buffer's final size to @p new_value.
     * @throws std::invalid_argument if <tt>new_value > maximum_size()</tt>.
     */
    void final_size(size_t new_value);

    /**
     * @brief Returns the difference between {@link final_size()}
     *        and {@link size()}.
     */
    inline size_t remaining() const;

    /**
     * @brief Returns the buffer's content.
     */
    inline const void* data() const;

    /**
     * @brief Returns the buffer's content.
     */
    inline void* data();

    /**
     * @brief Returns the buffer's content offset by @p offset.
     */
    inline void* offset_data(size_t offset);

    /**
     * @brief Returns the buffer's content offset by @p offset.
     */
    inline const void* offset_data(size_t offset) const;

    /**
     * @brief Checks whether <tt>size() == remaining()</tt>.
     */
    inline bool full() const;

    /**
     * @brief Checks whether <tt>size() == 0</tt>.
     */
    inline bool empty() const;

    /**
     * @brief Returns the maximum size of this buffer.
     */
    inline size_t maximum_size() const;

    /**
     * @brief Sets the maximum size of this buffer.
     */
    inline void maximum_size(size_t new_value);

    /**
     * @brief Writes @p num_bytes from @p data to this buffer.
     * @note The configured final size is ignored
     *       if <tt>wp == grow_if_needed</tt>.
     */
    void write(size_t num_bytes, const void* data, buffer_write_policy wp = grow_if_needed);

    /**
     * @brief Writes the content of @p other to this buffer.
     * @note The configured final size is ignored
     *       if <tt>wp == grow_if_needed</tt>.
     */
    void write(const buffer& other, buffer_write_policy wp = grow_if_needed);

    /**
     * @brief Writes the content of @p other to this buffer.
     * @note The configured final size is ignored
     *       if <tt>wp == grow_if_needed</tt>.
     */
    void write(buffer&& other, buffer_write_policy wp = grow_if_needed);

    /**
     * @brief Appends up to @p remaining() bytes from @p istream to the buffer.
     */
    void append_from(io::input_stream* istream);

    /**
     * @brief Returns the number of bytes used as minimal allocation unit.
     * @note The default chunk size is 512 bytes.
     */
    inline size_t chunk_size() const;

    /**
     * @brief Sets the number of bytes used as minimal allocation unit.
     * @note The default chunk size is 512 bytes.
     */
    inline void chunk_size(size_t new_value);

    /**
     * @brief Returns the number of currently allocated bytes.
     */
    inline size_t allocated() const;

 private:

    // pointer to the current write position
    inline char* wr_ptr();

    // called whenever bytes are written to m_data
    inline void inc_size(size_t value);

    // called from erase_* functions to adjust buffer size
    inline void dec_size(size_t value);

    // adjusts alloc_size according to m_chunk_size
    inline size_t adjust(size_t alloc_size) const;

    char*  m_data;
    size_t m_written;
    size_t m_allocated;
    size_t m_final_size;
    size_t m_chunk_size;
    size_t m_max_buffer_size;

};

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/


inline void buffer::clear() {
    m_written = 0;
}

inline size_t buffer::size() const {
    return m_written;
}

inline size_t buffer::final_size() const {
    return m_final_size;
}

inline size_t buffer::remaining() const {
    return m_final_size - m_written;
}

inline const void* buffer::data() const {
    return m_data;
}

inline void* buffer::data() {
    return m_data;
}

inline void* buffer::offset_data(size_t offset) {
    return m_data + offset;
}

inline const void* buffer::offset_data(size_t offset) const {
    return m_data + offset;
}

inline bool buffer::full() const {
    return remaining() == 0;
}

inline bool buffer::empty() const {
    return size() == 0;
}

inline size_t buffer::maximum_size() const {
    return m_max_buffer_size;
}

inline void buffer::maximum_size(size_t new_value) {
    m_max_buffer_size = new_value;
}

inline char* buffer::wr_ptr() {
    return m_data + m_written;
}

inline void buffer::inc_size(size_t value) {
    m_written += value;
}

inline void buffer::dec_size(size_t value) {
    m_written -= value;
}

inline size_t buffer::chunk_size() const {
    return m_chunk_size;
}

inline void buffer::chunk_size(size_t new_value) {
    m_chunk_size = new_value;
}

inline size_t buffer::allocated() const {
    return m_allocated;
}

inline size_t buffer::adjust(size_t alloc_size) const {
    auto remainder = (alloc_size % m_chunk_size);
    return (remainder == 0) ? alloc_size
                            : (alloc_size - remainder) + m_chunk_size;
}

} // namespace util
} // namespace cppa

#endif // CPPA_UTIL_BUFFER_HPP
