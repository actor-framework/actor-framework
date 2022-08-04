// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/ssl/fwd.hpp"

#include <memory>
#include <type_traits>

namespace caf::net::http {

/// Convenience function for creating async resources for connecting the HTTP
/// server to a worker.
inline auto make_request_resource() {
  return async::make_spsc_buffer_resource<request>();
}

/// Listens for incoming HTTP requests.
/// @param sys The host system.
/// @param fd An accept socket in listening mode, already bound to a port.
/// @param out A buffer resource that connects the server to a listener that
///            processes the requests.
/// @param cfg Optional configuration parameters for the HTTP layer.
disposable CAF_NET_EXPORT serve(actor_system& sys, tcp_accept_socket fd,
                                async::producer_resource<request> out,
                                const settings& cfg = {});

/// Listens for incoming HTTPS requests.
/// @param sys The host system.
/// @param acc An SSL connection acceptor with a socket that in listening mode.
/// @param out A buffer resource that connects the server to a listener that
///            processes the requests.
/// @param cfg Optional configuration parameters for the HTTP layer.
disposable CAF_NET_EXPORT serve(actor_system& sys, ssl::acceptor acc,
                                async::producer_resource<request> out,
                                const settings& cfg = {});

} // namespace caf::net::http
