// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/header.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/uri.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// Encapsulates meta data for HTTP requests. This class represents an HTTP
/// request header, providing methods for accessing the HTTP method, path,
/// query, fragment, version, and fields.
class CAF_NET_EXPORT request_header : public header {
public:
  using super = header;

  /// Default constructor.
  request_header() = default;

  /// Move constructor.
  request_header(request_header&&) = default;

  /// Copy constructor.
  request_header(const request_header&);

  /// Copy assignment operator.
  request_header& operator=(const request_header&);

  /// Clears the header content and fields.
  void clear() noexcept override;

  /// Returns the HTTP method of the request.
  http::method method() const noexcept {
    return method_;
  }

  /// Returns the path part of the request URI.
  std::string_view path() const noexcept {
    return uri_.path();
  }

  /// Returns the query part of the request URI as a map.
  const uri::query_map& query() const noexcept {
    return uri_.query();
  }

  /// Returns the fragment part of the request URI.
  std::string_view fragment() const noexcept {
    return uri_.fragment();
  }

  /// Returns the HTTP version of the request.
  std::string_view version() const noexcept {
    return version_;
  }

  /// Parses a raw request header string and returns a pair containing the
  /// status and a description for the status.
  /// @returns `status::bad_request` on error with a human-readable description
  ///          of the error, `status::ok` otherwise.
  std::pair<status, std::string_view> parse(std::string_view raw);

private:
  /// Stores the HTTP method that we've parsed from the raw input.
  http::method method_;

  /// Stores the HTTP request URI that we've parsed from the raw input.
  uri uri_;

  /// Stores the Version of the parsed HTTP input.
  std::string_view version_;
};

} // namespace caf::net::http
