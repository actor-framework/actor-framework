// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/web_socket/fwd.hpp"

namespace caf::net::web_socket {

// TODO: Documentation.
class upper_layer {
public:
  virtual ~upper_layer();

  virtual error
  init(net::socket_manager* mgr, lower_layer* down, const settings& cfg)
    = 0;
  virtual bool prepare_send() = 0;
  virtual bool done_sending() = 0;
  virtual void continue_reading() = 0;
  virtual void abort(const error& reason) = 0;
  virtual ptrdiff_t consume_binary(byte_span buf) = 0;
  virtual ptrdiff_t consume_text(std::string_view buf) = 0;
};

} // namespace caf::net::web_socket
