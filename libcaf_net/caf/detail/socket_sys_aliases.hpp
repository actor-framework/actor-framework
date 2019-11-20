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
