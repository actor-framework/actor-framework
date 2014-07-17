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

#ifndef CAF_IO_SYSTEM_MESSAGES_HPP
#define CAF_IO_SYSTEM_MESSAGES_HPP

#include "caf/io/handle.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/connection_handle.hpp"

namespace caf {
namespace io {

/**
 * @brief Signalizes a newly accepted connection from a {@link broker}.
 */
struct new_connection_msg {
    /**
     * @brief The handle that accepted the new connection.
     */
    accept_handle source;
    /**
     * @brief The handle for the new connection.
     */
    connection_handle handle;
};

inline bool operator==(const new_connection_msg& lhs,
                       const new_connection_msg& rhs) {
    return lhs.source == rhs.source && lhs.handle == rhs.handle;
}

inline bool operator!=(const new_connection_msg& lhs,
                       const new_connection_msg& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Signalizes newly arrived data for a {@link broker}.
 */
struct new_data_msg {
    /**
     * @brief Handle to the related connection.
     */
    connection_handle handle;
    /**
     * @brief Buffer containing the received data.
     */
    std::vector<char> buf;
};

inline bool operator==(const new_data_msg& lhs, const new_data_msg& rhs) {
    return lhs.handle == rhs.handle && lhs.buf == rhs.buf;
}

inline bool operator!=(const new_data_msg& lhs, const new_data_msg& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Signalizes that a {@link broker} connection has been closed.
 */
struct connection_closed_msg {
    /**
     * @brief Handle to the closed connection.
     */
    connection_handle handle;
};

inline bool operator==(const connection_closed_msg& lhs,
                       const connection_closed_msg& rhs) {
    return lhs.handle == rhs.handle;
}

inline bool operator!=(const connection_closed_msg& lhs,
                       const connection_closed_msg& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Signalizes that a {@link broker} acceptor has been closed.
 */
struct acceptor_closed_msg {
    /**
     * @brief Handle to the closed connection.
     */
    accept_handle handle;
};

inline bool operator==(const acceptor_closed_msg& lhs,
                       const acceptor_closed_msg& rhs) {
    return lhs.handle == rhs.handle;
}

inline bool operator!=(const acceptor_closed_msg& lhs,
                       const acceptor_closed_msg& rhs) {
    return !(lhs == rhs);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_SYSTEM_MESSAGES_HPP
