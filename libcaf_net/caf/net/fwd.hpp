// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>

#include "caf/intrusive_ptr.hpp"
#include "caf/type_id.hpp"

namespace caf::net {

// -- templates ----------------------------------------------------------------

template <class UpperLayer>
class stream_transport;

template <class Factory>
class datagram_transport;

template <class Application, class IdType = unit_t>
class transport_worker;

template <class Transport, class IdType = unit_t>
class transport_worker_dispatcher;

template <class... Sigs>
class typed_actor_shell;

template <class... Sigs>
class typed_actor_shell_ptr;

// -- classes ------------------------------------------------------------------

class actor_shell;
class actor_shell_ptr;
class middleman;
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

// -- smart pointer aliases ----------------------------------------------------

using multiplexer_ptr = std::shared_ptr<multiplexer>;
using socket_manager_ptr = intrusive_ptr<socket_manager>;
using weak_multiplexer_ptr = std::weak_ptr<multiplexer>;

} // namespace caf::net
