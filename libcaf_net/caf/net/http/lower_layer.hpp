// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/fwd.hpp"

namespace caf::net::http {

/// Parses HTTP requests and passes them to the upper layer.
class lower_layer {
public:
  virtual ~lower_layer();

  /// Queries whether the output device can accept more data straight away.
  virtual bool can_send_more() const noexcept = 0;

  /// Stops reading until manually calling `continue_reading` on the socket
  /// manager.
  virtual void suspend_reading() = 0;

  /// Sends the next header to the client.
  virtual bool
  send_header(context ctx, status code, const header_fields_map& fields)
    = 0;

  /// Sends the payload after the header.
  virtual bool send_payload(context, const_byte_span bytes) = 0;

  /// Sends a chunk of data if the full payload is unknown when starting to
  /// send.
  virtual bool send_chunk(context, const_byte_span bytes) = 0;

  /// Sends the last chunk, completing a chunked payload.
  virtual bool send_end_of_chunks() = 0;

  /// Terminates an HTTP context.
  virtual void fin(context) = 0;

  /// Convenience function for sending header and payload. Automatically sets
  /// the header fields `Content-Type` and `Content-Length`.
  bool send_response(context ctx, status codek, std::string_view content_type,
                     const_byte_span content);
};

} // namespace caf::net::http
