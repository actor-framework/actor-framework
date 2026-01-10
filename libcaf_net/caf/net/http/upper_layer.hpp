// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/generic_upper_layer.hpp"
#include "caf/net/http/lower_layer.hpp"

#include "caf/error.hpp"
#include "caf/fwd.hpp"

namespace caf::net::http {

/// Operates on HTTP requests.
class CAF_NET_EXPORT upper_layer : public generic_upper_layer {
public:
  class server;

  class client;

  ~upper_layer() override;
};

class CAF_NET_EXPORT upper_layer::server : public upper_layer {
public:
  ~server() override;

  /// Consumes an HTTP message.
  /// @param hdr The header fields for the received message.
  /// @param payload The payload of the received message.
  /// @returns The number of consumed bytes or a negative value to signal an
  ///          error.
  /// @note Discarded data is lost permanently.
  virtual ptrdiff_t consume(const request_header& hdr, const_byte_span payload)
    = 0;

  /// Begins a chunked message.
  /// @param hdr The header fields for the incoming message.
  /// @returns An error if the message cannot be started.
  virtual error begin_chunked_message(const request_header& hdr) = 0;

  /// Consumes a chunk of the message.
  /// @param payload The payload of the received chunk.
  /// @returns An error if the chunk cannot be consumed, or an empty error on
  ///          success.
  virtual error consume_chunk(const_byte_span payload) = 0;

  /// Ends a chunked message.
  /// @returns An error if the message cannot be ended.
  virtual error end_chunked_message() = 0;

  /// Initializes the upper layer.
  /// @param down A pointer to the lower layer that remains valid for the
  ///             lifetime of the upper layer.
  virtual error start(lower_layer::server* down) = 0;
};

class CAF_NET_EXPORT upper_layer::client : public upper_layer {
public:
  ~client() override;

  /// Consumes an HTTP message.
  /// @param hdr The header fields for the received message.
  /// @param payload The payload of the received message.
  /// @returns The number of consumed bytes or a negative value to signal an
  ///          error.
  /// @note Discarded data is lost permanently.
  virtual ptrdiff_t consume(const response_header& hdr, const_byte_span payload)
    = 0;

  /// Initializes the upper layer.
  /// @param down A pointer to the lower layer that remains valid for the
  ///             lifetime of the upper layer.
  virtual error start(lower_layer::client* down) = 0;
};

} // namespace caf::net::http
