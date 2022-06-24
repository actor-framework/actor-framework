// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_upper_layer.hpp"
#include "caf/net/web_socket/fwd.hpp"

namespace caf::net::web_socket {

/// Consumes text and binary messages from the lower layer.
class CAF_NET_EXPORT upper_layer : public generic_upper_layer {
public:
  virtual ~upper_layer();

  virtual error start(lower_layer* down, const settings& cfg) = 0;
  virtual ptrdiff_t consume_binary(byte_span buf) = 0;
  virtual ptrdiff_t consume_text(std::string_view buf) = 0;
};

} // namespace caf::net::web_socket
