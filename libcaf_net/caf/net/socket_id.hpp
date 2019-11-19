/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>

#include "caf/config.hpp"

namespace caf::net {

#ifdef CAF_WINDOWS

/// Platform-specific representation of a socket.
/// @relates socket
using socket_id = size_t;

/// Identifies the invalid socket.
constexpr socket_id invalid_socket_id = std::numeric_limits<socket_id>::max();

#else // CAF_WINDOWS

/// Platform-specific representation of a socket.
/// @relates socket
using socket_id = int;

/// Identifies the invalid socket.
constexpr socket_id invalid_socket_id = -1;

#endif // CAF_WINDOWS

/// Signed counterpart of `socket_id`.
/// @relates socket
using signed_socket_id = std::make_signed<socket_id>::type;

} // namespace caf
