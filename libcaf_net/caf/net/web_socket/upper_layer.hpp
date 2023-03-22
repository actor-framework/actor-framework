// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_upper_layer.hpp"
#include "caf/net/http/request_header.hpp"

namespace caf::net::web_socket {

/// Consumes text and binary messages from the lower layer.
/// @note This interface gets refined further depending on whether to implement
///       a server or a client.
class CAF_NET_EXPORT upper_layer : public generic_upper_layer {
public:
  class server;

  class client;

  virtual ~upper_layer();

  virtual ptrdiff_t consume_binary(byte_span buf) = 0;

  virtual ptrdiff_t consume_text(std::string_view buf) = 0;
};

class upper_layer::server : public upper_layer {
public:
  virtual ~server();
  virtual error start(lower_layer* down, const http::request_header& hdr) = 0;
};

class upper_layer::client : public upper_layer {
public:
  virtual ~client();
  virtual error start(lower_layer* down) = 0;
};

} // namespace caf::net::web_socket
