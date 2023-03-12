// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

template <class, class...>
class ws_flow_connector_request_impl;

} // namespace caf::detail

namespace caf::net::web_socket {

template <class... Ts>
class acceptor;

template <class Trait, class... Ts>
class acceptor_impl;

template <class Trait, class... Ts>
class server_factory;

class frame;
class framing;
class lower_layer;
class server;
class upper_layer;

enum class status : uint16_t;

} // namespace caf::net::web_socket
