// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_lower_layer.hpp"
#include "caf/net/web_socket/fwd.hpp"

#include <string_view>

namespace caf::net::web_socket {

/// Produces text and binary messages for the upper layer.
class CAF_NET_EXPORT lower_layer : public generic_lower_layer {
public:
  virtual ~lower_layer();

  /// Pulls messages from the transport until calling `suspend_reading`.
  virtual void request_messages() = 0;

  /// Begins transmission of a binary message.
  virtual void begin_binary_message() = 0;

  /// Returns the buffer for the current binary message. Must be called between
  /// calling `begin_binary_message` and `end_binary_message`.
  virtual byte_buffer& binary_message_buffer() = 0;

  /// Seals the current binary message buffer and ships a new WebSocket frame.
  virtual bool end_binary_message() = 0;

  /// Begins transmission of a text message.
  virtual void begin_text_message() = 0;

  /// Returns the buffer for the current text message. Must be called between
  /// calling `begin_text_message` and `end_text_message`.
  virtual text_buffer& text_message_buffer() = 0;

  /// Seals the current text message buffer and ships a new WebSocket frame.
  virtual bool end_text_message() = 0;

  /// Sends the close message with @ref status `normal_close`.
  virtual void send_close_message() = 0;

  /// Sends the close message with an custom @ref status.
  virtual void send_close_message(status code, std::string_view desc) = 0;

  /// Sends the close message with @ref status `unexpected_condition`.
  void send_close_message(const error& reason);
};

} // namespace caf::net::web_socket
