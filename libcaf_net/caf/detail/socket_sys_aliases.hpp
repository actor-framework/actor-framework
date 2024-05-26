// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

namespace caf::net {

#ifdef CAF_WINDOWS

using setsockopt_ptr = const char*;
using getsockopt_ptr = char*;
using socket_send_ptr = const char*;
using socket_recv_ptr = char*;
using socket_size_type = int;

#else // CAF_WINDOWS

using setsockopt_ptr = const void*;
using getsockopt_ptr = void*;
using socket_send_ptr = const void*;
using socket_recv_ptr = void*;
using socket_size_type = unsigned;

#endif // CAF_WINDOWS

} // namespace caf::net
