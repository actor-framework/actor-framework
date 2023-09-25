// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
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
class CAF_NET_EXPORT response_header {
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
  http::status status() const noexcept {
    return status_;
  }

  /// Returns the HTTP status text of the request.
  std::string_view status_text() const noexcept {
    return status_text_;
  }

  /// Returns the number of fields in the request header.
  size_t num_fields() const noexcept {
    return fields_.size();
  }

  /// Returns the field at the specified index as a key-value pair.
  std::pair<std::string_view, std::string_view> field_at(size_t index) const {
    return fields_.at(index);
  }

  /// Checks if the request header has a field with the specified key.
  bool has_field(std::string_view key) const noexcept {
    return find_by_key_icase(key) != fields_.end();
  }

  /// Returns the value of the field with the specified key, or an empty view if
  /// the field is not found.
  std::string_view field(std::string_view key) const noexcept {
    if (auto i = find_by_key_icase(key); i != fields_.end())
      return i->second;
    else
      return {};
  }

  ///  Checks whether the field `key` exists and equals `val` when using
  ///  case-insensitive compare of the value.
  bool field_equals(ignore_case_t, std::string_view key,
                    std::string_view val) const noexcept {
    if (auto i = find_by_key_icase(key); i != fields_.end())
      return icase_equal(val, i->second);
    else
      return false;
  }

  ///  Checks whether the field `key` exists and equals `val` when using
  ///  case-sensitive compare of the value.
  bool field_equals(std::string_view key, std::string_view val) const noexcept {
    if (auto i = find_by_key_icase(key); i != fields_.end())
      return val == i->second;
    else
      return false;
  }

  /// Returns the value of the field with the specified key as the requested
  /// type T, or std::nullopt if the field is not found or cannot be converted.
  template <class T>
  std::optional<T> field_as(std::string_view key) const noexcept {
    if (auto i = find_by_key_icase(key); i != fields_.end()) {
      caf::config_value val{std::string{i->second}};
      if (auto res = caf::get_as<T>(val))
        return std::move(*res);
    }
    return std::nullopt;
  }

  /// Executes the provided callable `f` for each field in the request header.
  /// @param f A function object taking two `std::string_view` parameters: key
  ///          and value.
  template <class F>
  void for_each_field(F&& f) const {
    for (auto& [key, val] : fields_)
      f(key, val);
  }

  std::string_view body() const noexcept {
    return body_;
  }

  /// Checks if the request header is valid (non-empty).
  bool valid() const noexcept {
    return !raw_.empty();
  }

  /// Convenience function for `field_as<size_t>("Content-Length")`.
  std::optional<size_t> content_length() const;

  /// Checks whether the client has defined `Transfer-Encoding` as `chunked`.
  bool chunked_transfer_encoding() const;

  /// Parses a raw response header string and returns a pair containing the
  /// status and a description for the status.
  /// @returns `status::bad_request` on error with a human-readable description
  ///          of the error, `status::ok` otherwise.
  std::pair<http::status, std::string_view> parse(std::string_view raw);

private:
  // An unsorted "map" type for storing key/value pairs.
  using fields_map = std::vector<std::pair<std::string_view, std::string_view>>;

  // Finds a field by using case insensitive key comparison. Returns the
  // iterator pointing to the found field, or to the end if no field is found.
  fields_map::const_iterator
  find_by_key_icase(std::string_view key) const noexcept {
    return std::find_if(fields_.begin(), fields_.end(), [&key](const auto& i) {
      return icase_equal(i.first, key);
    });
  }

  /// Stores the raw HTTP input.
  std::vector<char> raw_;

  /// Stores the version of the parsed HTTP input.
  std::string_view version_;

  /// Stores the status of the parsed HTTP input.
  http::status status_;

  /// Stores the status text of the parsed HTTP input.
  std::string_view status_text_;

  /// A shallow map for looking up individual header fields.
  fields_map fields_;

  /// Stores the body of the parsed HTTP input.
  std::string_view body_;
};

} // namespace caf::net::http
