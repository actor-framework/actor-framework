// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_lower_layer.hpp"
#include "caf/net/http/fwd.hpp"

namespace caf::net::http {

/// Parses HTTP requests and passes them to the upper layer.
class CAF_NET_EXPORT lower_layer : public generic_lower_layer {
public:
  virtual ~lower_layer();

  /// Start or re-start reading data from the client.
  virtual void request_messages() = 0;

  /// Stops reading messages until calling `request_messages`.
  virtual void suspend_reading() = 0;

  /// Sends the next header to the client.
  virtual bool send_header(status code, const header_fields_map& fields) = 0;

  /// Sends the payload after the header.
  virtual bool send_payload(const_byte_span bytes) = 0;

  /// Sends a chunk of data if the full payload is unknown when starting to
  /// send.
  virtual bool send_chunk(const_byte_span bytes) = 0;

  /// Sends the last chunk, completing a chunked payload.
  virtual bool send_end_of_chunks() = 0;

  /// Convenience function for sending header and payload. Automatically sets
  /// the header fields `Content-Type` and `Content-Length`.
  bool send_response(status code, std::string_view content_type,
                     const_byte_span content);

  /// @copydoc send_response
  bool send_response(status code, std::string_view content_type,
                     std::string_view content);
};

} // namespace caf::net::http
