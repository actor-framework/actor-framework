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

#include <memory>

#include "caf/intrusive_ptr.hpp"

namespace caf {
namespace net {

class multiplexer;
class socket_manager;
template <class Application, class IdType = unit_t>
class transport_worker;
template <class Application, class IdType>
class transport_worker_dispatcher;

struct network_socket;
struct pipe_socket;
struct socket;
struct stream_socket;
struct tcp_stream_socket;
struct tcp_accept_socket;

using socket_manager_ptr = intrusive_ptr<socket_manager>;

using multiplexer_ptr = std::shared_ptr<multiplexer>;

using weak_multiplexer_ptr = std::weak_ptr<multiplexer>;

} // namespace net
} // namespace caf
