// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/http/response.hpp"

#include "caf/async/execution_context.hpp"
#include "caf/async/promise.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace caf::net::http {

/// Implicitly shared handle type that represents an HTTP client request with a
/// promise for the HTTP response.
class CAF_NET_EXPORT request {
public:
  friend class router;

  class impl;

  request() noexcept = default;

  request(request&& other) noexcept;

  request(const request& other) noexcept;

  request& operator=(request&& other) noexcept;

  request& operator=(const request& other) noexcept;

  ~request();

  /// Returns the HTTP header for the request.
  /// @pre `valid()`
  const request_header& header() const;

  /// Returns the HTTP body (payload) for the request.
  /// @pre `valid()`
  const_byte_span body() const;

  /// @copydoc body
  const_byte_span payload() const;

  /// Sends an HTTP response message to the client. Automatically sets the
  /// `Content-Type` and `Content-Length` header fields.
  /// @pre `valid()`
  void respond(status code, std::string_view content_type,
               const_byte_span content) const;

  /// Sends an HTTP response message to the client. Automatically sets the
  /// `Content-Type` and `Content-Length` header fields.
  /// @pre `valid()`
  void respond(status code, std::string_view content_type,
               std::string_view content) const {
    return respond(code, content_type, as_bytes(make_span(content)));
  }

private:
  request(request_header hdr, std::vector<std::byte> body,
          async::promise<response> prom);

  impl* impl_ = nullptr;
};

} // namespace caf::net::http
