// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/header_fields.hpp"
#include "caf/net/http/status.hpp"

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/uri.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// Encapsulates meta data for HTTP response. This class represents a HTTP
/// response header, providing methods for accessing the HTTP version, status,
/// fields and body.
class CAF_NET_EXPORT response_header : public header_fields {
public:
  /// Default constructor.
  response_header() = default;

  /// Move constructor.
  response_header(response_header&&) = default;

  /// Move assignment operator.
  response_header& operator=(response_header&&) = default;

  /// Copy constructor.
  response_header(const response_header&);

  /// Copy assignment operator.
  response_header& operator=(const response_header&);

  /// Assigns the content of another response_header.
  void assign(const response_header&);

  /// Returns the HTTP version of the request.
  std::string_view version() const noexcept {
    return version_;
  }

  /// Returns the HTTP status of the request.
  uint16_t status() const noexcept {
    return status_;
  }

  /// Returns the HTTP status text of the request.
  std::string_view status_text() const noexcept {
    return status_text_;
  }

  /// Returns the HTTP body of the request.
  std::string_view body() const noexcept {
    return body_;
  }

  /// Parses a raw response header string and returns a pair containing the
  /// status and a description for the status.
  /// @returns `status::bad_request` on error with a human-readable description
  ///          of the error, `status::ok` otherwise.
  std::pair<http::status, std::string_view> parse(std::string_view raw);

private:
  /// Stores the version of the parsed HTTP input.
  std::string_view version_;

  /// Stores the status of the parsed HTTP input.
  uint16_t status_;

  /// Stores the status text of the parsed HTTP input.
  std::string_view status_text_;

  /// Stores the body of the parsed HTTP input.
  std::string_view body_;
};

} // namespace caf::net::http
