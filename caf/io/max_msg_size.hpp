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

#ifndef CAF_IO_MAX_MSG_SIZE_HPP
#define CAF_IO_MAX_MSG_SIZE_HPP

#include <cstddef> // size_t

namespace caf {
namespace io {

/**
 * @brief Sets the maximum size of a message over network.
 * @param size The maximum number of bytes a message may occupy.
 */
void max_msg_size(size_t size);

/**
 * @brief Queries the maximum size of messages over network.
 * @returns The number maximum number of bytes a message may occupy.
 */
size_t max_msg_size();

} // namespace io
} // namespace caf

#endif // CAF_IO_MAX_MSG_SIZE_HPP
