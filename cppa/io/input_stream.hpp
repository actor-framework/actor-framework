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


#ifndef CPPA_IO_INPUT_STREAM_HPP
#define CPPA_IO_INPUT_STREAM_HPP

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {
namespace io {

/**
 * @brief An abstract input stream interface.
 */
class input_stream : public virtual ref_counted {

 public:

    ~input_stream();

    /**
     * @brief Returns the internal file descriptor. This descriptor is needed
     *        for socket multiplexing using select().
     */
    virtual native_socket_type read_handle() const = 0;

    /**
     * @brief Reads exactly @p num_bytes from the data source and blocks the
     *        caller if needed.
     * @throws network_error
     */
    virtual void read(void* buf, size_t num_bytes) = 0;

    /**
     * @brief Tries to read up to @p num_bytes from the data source.
     * @returns The number of read bytes.
     * @throws network_error
     */
    virtual size_t read_some(void* buf, size_t num_bytes) = 0;

};

/**
 * @brief An input stream pointer.
 */
typedef intrusive_ptr<input_stream> input_stream_ptr;

} // namespace io
} // namespace cppa

#endif // CPPA_IO_INPUT_STREAM_HPP
