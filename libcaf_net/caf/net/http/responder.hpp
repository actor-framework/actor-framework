// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/request_header.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"

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
  // -- member types -----------------------------------------------------------

  /// Implementation detail for `promise`.
  class CAF_NET_EXPORT promise_state : public ref_counted {
  public:
    explicit promise_state(lower_layer::server* down) : down_(down) {
      // nop
    }

    promise_state(const promise_state&) = delete;

    promise_state& operator-(const promise_state&) = delete;

    ~promise_state() override;

    /// Returns a pointer to the HTTP layer.
    lower_layer::server* down() {
      return down_;
    }

    /// Marks the promise as fulfilled
    void set_completed() {
      completed_ = true;
    }

  private:
    lower_layer::server* down_;
    bool completed_ = false;
  };

  /// Allows users to respond to an incoming HTTP request at some later time.
  class CAF_NET_EXPORT promise {
  public:
    explicit promise(responder& parent);

    promise(const promise&) noexcept = default;

    promise& operator=(const promise&) noexcept = default;

    /// Sends an HTTP response that only consists of a header with a status code
    /// such as `status::no_content`.
    void respond(status code) {
      impl_->down()->send_response(code);
      impl_->set_completed();
    }

    /// Sends an HTTP response message to the client. Automatically sets the
    /// `Content-Type` and `Content-Length` header fields.
    void respond(status code, std::string_view content_type,
                 const_byte_span content) {
      impl_->down()->send_response(code, content_type, content);
      impl_->set_completed();
    }

    /// Sends an HTTP response message to the client. Automatically sets the
    /// `Content-Type` and `Content-Length` header fields.
    void respond(status code, std::string_view content_type,
                 std::string_view content) {
      impl_->down()->send_response(code, content_type, content);
      impl_->set_completed();
    }

    /// Sends an HTTP response message with an error to the client.
    /// Converts @p what to a string representation and then transfers this
    /// description to the client.
    void respond(status code, const error& what) {
      impl_->down()->send_response(code, what);
      impl_->set_completed();
    }

    /// Returns a pointer to the HTTP layer.
    lower_layer::server* down() {
      return impl_->down();
    }

  private:
    intrusive_ptr<promise_state> impl_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  responder(const request_header* hdr, const_byte_span body,
            http::router* router)
    : hdr_(hdr), body_(body), router_(router) {
    // nop
  }

  responder(const responder&) noexcept = default;

  responder& operator=(const responder&) noexcept = default;

  // -- properties -------------------------------------------------------------

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

  /// Returns the @ref actor_shell object from the @ref router for interacting
  /// with actors in the system.
  actor_shell* self();

  /// Returns a pointer to the HTTP layer.
  lower_layer::server* down();

  // -- responding -------------------------------------------------------------

  /// Sends an HTTP response that only consists of a header with a status code
  /// such as `status::no_content`.
  void respond(status code) {
    down()->send_response(code);
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

  /// Sends an HTTP response message with an error to the client.
  /// Converts @p what to a string representation and then transfers this
  /// description to the client.
  void respond(status code, const error& what) {
    down()->send_response(code, what);
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

  // -- conversions ------------------------------------------------------------

  /// Converts a responder to a @ref request for processing the HTTP request
  /// asynchronously.
  request to_request() &&;

  /// Converts the responder to a promise object for responding to the HTTP
  /// request at some later time but from the same @ref socket_manager.
  promise to_promise() &&;

private:
  const request_header* hdr_;
  const_byte_span body_;
  http::router* router_;
};

} // namespace caf::net::http
