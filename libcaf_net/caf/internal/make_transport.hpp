// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/stream_socket.hpp"

#include <memory>

namespace caf::internal {

inline auto
make_transport(net::stream_socket fd,
               std::unique_ptr<net::octet_stream::upper_layer> upper_layer) {
  return net::octet_stream::transport::make(std::move(fd),
                                            std::move(upper_layer));
}

inline auto
make_transport(net::ssl::connection conn,
               std::unique_ptr<net::octet_stream::upper_layer> upper_layer) {
  return net::ssl::transport::make(std::move(conn), std::move(upper_layer));
}

} // namespace caf::internal
