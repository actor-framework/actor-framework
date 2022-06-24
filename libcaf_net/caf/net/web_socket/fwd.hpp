// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::net::web_socket {

template <class OnRequest, class Trait, class... Ts>
class flow_connector_request_impl;

template <class Trait>
class flow_bridge;

template <class Trait, class... Ts>
class request;

class frame;
class framing;
class lower_layer;
class server;
class upper_layer;

enum class status : uint16_t;

} // namespace caf::net::web_socket
