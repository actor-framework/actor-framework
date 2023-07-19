// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <cstddef>
#include <limits>
#include <type_traits>

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

} // namespace caf::net
