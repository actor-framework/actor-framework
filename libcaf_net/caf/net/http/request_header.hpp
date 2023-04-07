// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/uri.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// Encapsulates meta data for HTTP requests. This class represents an HTTP
/// request header, providing methods for accessing the HTTP method, path,
/// query, fragment, version, and fields.
class CAF_NET_EXPORT request_header {
public:
  friend class http::server;
  friend class web_socket::server;

  /// Default constructor.
  request_header() = default;

  /// Move constructor.
  request_header(request_header&&) = default;

  /// Move assignment operator.
  request_header& operator=(request_header&&) = default;

  /// Copy constructor.
  request_header(const request_header&);

  /// Copy assignment operator.
  request_header& operator=(const request_header&);

  /// Assigns the content of another request_header.
  void assign(const request_header&);

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

  /// Returns the number of fields in the request header.
  size_t num_fields() const noexcept {
    return fields_.size();
  }

  /// Returns the field at the specified index as a key-value pair.
  std::pair<std::string_view, std::string_view> field_at(size_t index) {
    return fields_.container().at(index);
  }

  /// Checks if the request header has a field with the specified key.
  bool has_field(std::string_view key) const noexcept {
    return fields_.find(key) != fields_.end();
  }

  /// Returns the value of the field with the specified key, or an empty view if
  /// the field is not found.
  std::string_view field(std::string_view key) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end())
      return i->second;
    else
      return {};
  }

  ///  Checks whether the field `key` exists and equals `val` when using
  ///  case-insensitive compare.
  bool field_equals(ignore_case_t, std::string_view key,
                    std::string_view val) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end())
      return icase_equal(val, i->second);
    else
      return false;
  }

  ///  Checks whether the field `key` exists and equals `val` when using
  ///  case-insensitive compare.
  bool field_equals(std::string_view key, std::string_view val) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end())
      return val == i->second;
    else
      return false;
  }

  /// Returns the value of the field with the specified key as the requested
  /// type T, or std::nullopt if the field is not found or cannot be converted.
  template <class T>
  std::optional<T> field_as(std::string_view key) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end()) {
      caf::config_value val{std::string{i->second}};
      if (auto res = caf::get_as<T>(val))
        return std::move(*res);
    }
    return {};
  }

  /// Executes the provided callable `f` for each field in the request header.
  /// @param f A function object taking two `std::string_view` parameters: key
  ///          and value.
  template <class F>
  void for_each_field(F&& f) const {
    for (auto& [key, val] : fields_)
      f(key, val);
  }

  /// Checks if the request header is valid (non-empty).
  bool valid() const noexcept {
    return !raw_.empty();
  }

  /// Checks whether the client has defined `Transfer-Encoding` as `chunked`.
  bool chunked_transfer_encoding() const;

  /// Convenience function for `field_as<size_t>("Content-Length")`.
  std::optional<size_t> content_length() const;

  /// Parses a raw request header string and returns a pair containing the
  /// status and a description for the status.
  /// @returns `status::bad_request` on error with a human-readable description
  ///          of the error, `status::ok` otherwise.
  std::pair<status, std::string_view> parse(std::string_view raw);

private:
  /// Stores the raw HTTP input.
  std::vector<char> raw_;

  /// Stores the HTTP method that we've parsed from the raw input.
  http::method method_;

  /// Stores the HTTP request URI that we've parsed from the raw input.
  uri uri_;

  /// Stores the Version of the parsed HTTP input.
  std::string_view version_;

  /// A shallow map for looking up individual header fields.
  unordered_flat_map<std::string_view, std::string_view> fields_;
};

} // namespace caf::net::http
