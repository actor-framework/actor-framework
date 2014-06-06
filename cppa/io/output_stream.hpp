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


#ifndef CPPA_IO_OUTPUT_STREAM_HPP
#define CPPA_IO_OUTPUT_STREAM_HPP

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {
namespace io {

/**
 * @brief An abstract output stream interface.
 */
class output_stream : public virtual ref_counted {

 public:

    ~output_stream();

    /**
     * @brief Returns the internal file descriptor. This descriptor is needed
     *        for socket multiplexing using select().
     */
    virtual native_socket_type write_handle() const = 0;

    /**
     * @brief Writes @p num_bytes bytes from @p buf to the data sink.
     * @note This member function blocks until @p num_bytes were successfully
     *       written.
     * @throws network_error
     */
    virtual void write(const void* buf, size_t num_bytes) = 0;

    /**
     * @brief Tries to write up to @p num_bytes bytes from @p buf.
     * @returns The number of written bytes.
     * @throws network_error
     */
    virtual size_t write_some(const void* buf, size_t num_bytes) = 0;

};

/**
 * @brief An output stream pointer.
 */
typedef intrusive_ptr<output_stream> output_stream_ptr;

} // namespace io
} // namespace cppa

#endif // CPPA_IO_OUTPUT_STREAM_HPP
