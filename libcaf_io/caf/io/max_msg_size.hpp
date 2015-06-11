/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_MAX_MSG_SIZE_HPP
#define CAF_IO_MAX_MSG_SIZE_HPP

#include <cstddef> // size_t

namespace caf {
namespace io {

/// Sets the maximum size of a message over network.
/// @param size The maximum number of bytes a message may occupy.
void max_msg_size(size_t size);

/// Queries the maximum size of messages over network.
/// @returns The number maximum number of bytes a message may occupy.
size_t max_msg_size();

} // namespace io
} // namespace caf

#endif // CAF_IO_MAX_MSG_SIZE_HPP
