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


#ifndef CPPA_IO_CONTINUABLE_WRITER_HPP
#define CPPA_IO_CONTINUABLE_WRITER_HPP

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/io/event.hpp"

namespace cppa {
namespace io {

/**
 * @brief Denotes the return value of
 *        {@link continuable::continue_reading()}.
 */
enum class continue_reading_result {
    failure,
    closed,
    continue_later
};

/**
 * @brief Denotes the return value of
 *        {@link continuable::continue_writing()}.
 */
enum class continue_writing_result {
    failure,
    closed,
    continue_later,
    done
};

/**
 * @brief An object performing asynchronous input and output.
 */
class continuable {

    continuable(const continuable&) = delete;
    continuable& operator=(const continuable&) = delete;

 public:

    virtual ~continuable();

    /**
     * @brief Disposes this instance. This member function is invoked by
     *        the middleman once it has neither pending reads nor pending
     *        writes for this instance.
     *
     * This member function is expected to perform cleanup code
     * and to release memory, e.g., by calling <tt>delete this</tt>.
     */
    virtual void dispose() = 0;

    /**
     * @brief Returns the file descriptor for incoming data.
     */
    inline native_socket_type read_handle() const;

    /**
     * @brief Returns the file descriptor for outgoing data.
     */
    inline native_socket_type write_handle() const;

    /**
     * @brief Reads from {@link read_handle()} if valid.
     */
    virtual continue_reading_result continue_reading();

    /**
     * @brief Writes to {@link write_handle()} if valid.
     */
    virtual continue_writing_result continue_writing();

    /**
     * @brief Called from middleman before it removes this object
     *        due to an IO failure, can be called twice
     *        (for read and for write error).
     * @param bitmask Either @p read or @p write.
     */
     virtual void io_failed(event_bitmask bitmask) = 0;

 protected:

    continuable(native_socket_type read_fd,
                native_socket_type write_fd = invalid_socket);

 private:

    native_socket_type m_rd;
    native_socket_type m_wr;

};

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline native_socket_type continuable::read_handle() const {
    return m_rd;
}

/**
 * @brief Returns the file descriptor for outgoing data.
 */
inline native_socket_type continuable::write_handle() const {
    return m_wr;
}

} // namespace io
} // namespace cppa

#endif // CPPA_IO_CONTINUABLE_WRITER_HPP
