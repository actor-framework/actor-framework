// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/web_socket/fwd.hpp"

#include <string_view>

namespace caf::net::web_socket {

// TODO: Documentation.

class lower_layer {
public:
  virtual ~lower_layer();

  virtual bool can_send_more() = 0;
  virtual void suspend_reading() = 0;
  virtual void begin_binary_message() = 0;
  virtual byte_buffer& binary_message_buffer() = 0;
  virtual bool end_binary_message() = 0;
  virtual void begin_text_message() = 0;
  virtual text_buffer& text_message_buffer() = 0;
  virtual bool end_text_message() = 0;
  virtual void send_close_message() = 0;
  virtual void send_close_message(status code, std::string_view desc) = 0;
  void send_close_message(const error& reason);
};

} // namespace caf::net::web_socket
