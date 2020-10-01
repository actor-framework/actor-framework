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

namespace caf::net {

// -- templates ----------------------------------------------------------------

template <class Application>
class stream_transport;

template <class Factory>
class datagram_transport;

template <class Application, class IdType = unit_t>
class transport_worker;

template <class Transport, class IdType = unit_t>
class transport_worker_dispatcher;

enum class ec : uint8_t;

// -- classes ------------------------------------------------------------------

class actor_shell;
class actor_shell_ptr;
class endpoint_manager;
class middleman;
class middleman_backend;
class multiplexer;
class socket_manager;

// -- structs ------------------------------------------------------------------

struct network_socket;
struct pipe_socket;
struct socket;
struct stream_socket;
struct tcp_accept_socket;
struct tcp_stream_socket;
struct datagram_socket;
struct udp_datagram_socket;

// -- smart pointers -----------------------------------------------------------

using endpoint_manager_ptr = intrusive_ptr<endpoint_manager>;
using middleman_backend_ptr = std::unique_ptr<middleman_backend>;
using multiplexer_ptr = std::shared_ptr<multiplexer>;
using socket_manager_ptr = intrusive_ptr<socket_manager>;
using weak_multiplexer_ptr = std::weak_ptr<multiplexer>;

} // namespace caf::net

namespace caf::net::basp {

enum class ec : uint8_t;

} // namespace caf::net::basp

CAF_BEGIN_TYPE_ID_BLOCK(net_module, detail::net_module_begin)

  CAF_ADD_TYPE_ID(net_module, (caf::net::basp::ec))

CAF_END_TYPE_ID_BLOCK(net_module)

static_assert(caf::id_block::net_module::end == caf::detail::net_module_end);
