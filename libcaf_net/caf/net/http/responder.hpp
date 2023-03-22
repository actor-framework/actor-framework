// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/request_header.hpp"

#include <string_view>

namespace caf::net::http {

/// Responds to an HTTP request at the server. Provides functions for accessing
/// the HTTP client request and for writing the HTTP response.
///
/// This class has a similar API to @ref request, but is used at the @ref server
/// directly. While a @ref request is meant to handled outside of the server by
/// eventually fulfilling the response promise, a `responder` must generate the
/// response immediately.
class CAF_NET_EXPORT responder {
public:
  responder(const request_header* hdr, const_byte_span body,
            http::router* router)
    : hdr_(hdr), body_(body), router_(router) {
    // nop
  }

  responder(const responder&) noexcept = default;

  responder& operator=(const responder&) noexcept = default;

  /// Returns the HTTP header for the responder.
  /// @pre `valid()`
  const request_header& header() const noexcept {
    return *hdr_;
  }

  /// Returns the HTTP body (payload) for the responder.
  /// @pre `valid()`
  const_byte_span body() const noexcept {
    return body_;
  }

  /// @copydoc body
  const_byte_span payload() const noexcept {
    return body_;
  }

  /// Returns the router that has created this responder.
  http::router* router() const noexcept {
    return router_;
  }

  /// Sends an HTTP response message to the client. Automatically sets the
  /// `Content-Type` and `Content-Length` header fields.
  /// @pre `valid()`
  void respond(status code, std::string_view content_type,
               const_byte_span content) {
    down()->send_response(code, content_type, content);
  }

  /// Sends an HTTP response message to the client. Automatically sets the
  /// `Content-Type` and `Content-Length` header fields.
  /// @pre `valid()`
  void respond(status code, std::string_view content_type,
               std::string_view content) {
    down()->send_response(code, content_type, content);
  }

  /// Starts writing an HTTP header.
  void begin_header(status code) {
    down()->begin_header(code);
  }

  /// Adds a header field. Users may only call this function between
  /// `begin_header` and `end_header`.
  void add_header_field(std::string_view key, std::string_view val) {
    down()->add_header_field(key, val);
  }

  /// Seals the header and transports it to the client.
  bool end_header() {
    return down()->end_header();
  }

  /// Sends the payload after the header.
  bool send_payload(const_byte_span bytes) {
    return down()->send_payload(bytes);
  }

  /// Sends a chunk of data if the full payload is unknown when starting to
  /// send.
  bool send_chunk(const_byte_span bytes) {
    return down()->send_chunk(bytes);
  }

  /// Sends the last chunk, completing a chunked payload.
  bool send_end_of_chunks() {
    return down()->send_end_of_chunks();
  }

  /// Converts a responder to a @ref request for processing the HTTP request
  /// asynchronously.
  request to_request() &&;

private:
  lower_layer* down();

  const request_header* hdr_;
  const_byte_span body_;
  http::router* router_;
};

} // namespace caf::net::http
