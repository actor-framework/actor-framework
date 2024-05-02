// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/async/fwd.hpp"
#include "caf/detail/ws_conn_acceptor.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf::detail {

CAF_NET_EXPORT
std::unique_ptr<net::web_socket::upper_layer>
make_ws_flow_bridge(async::consumer_resource<net::web_socket::frame> pull,
                    async::producer_resource<net::web_socket::frame> push);

CAF_NET_EXPORT
std::unique_ptr<net::web_socket::upper_layer::server>
make_ws_flow_bridge(detail::ws_conn_acceptor_ptr wca);

} // namespace caf::detail
