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

#ifndef CAF_IO_PUBLISH_LOCAL_GROUPS_HPP
#define CAF_IO_PUBLISH_LOCAL_GROUPS_HPP

#include <cstdint>

namespace caf {
namespace io {

/// Makes *all* local groups accessible via network on address `addr` and `port`.
/// @returns The actual port the OS uses after `bind()`. If `port == 0` the OS
///          chooses a random high-level port.
/// @throws bind_failure
/// @throws network_error
uint16_t publish_local_groups(uint16_t port, const char* addr = nullptr);

} // namespace io
} // namespace caf

#endif // CAF_IO_PUBLISH_LOCAL_GROUPS_HPP
