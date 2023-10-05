// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/string_algorithms.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// Encapsulates meta data for HTTP header fields. This class represents a base
/// class used for HTTP request and response representations, each providing
/// additional message specific methods.
class CAF_NET_EXPORT header {
public:
  /// Virtual destructor.
  virtual ~header();

  /// Default constructor.
  header() = default;

  /// Move constructor.
  header(header&&) = default;

  /// Move assignment operator.
  header& operator=(header&&) = default;

  /// Copy constructor.
  header(const header&);

  /// Copy assignment operator.
  header& operator=(const header&);

  /// Clears the header content and fields.
  virtual void clear() noexcept;

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
    return {};
  }

  ///  Checks whether the field `key` exists and equals `val` when using
  ///  case-insensitive compare of the value.
  bool field_equals(ignore_case_t, std::string_view key,
                    std::string_view val) const noexcept {
    if (auto i = find_by_key_icase(key); i != fields_.end())
      return icase_equal(val, i->second);
    return false;
  }

  ///  Checks whether the field `key` exists and equals `val` when using
  ///  case-sensitive compare of the value.
  bool field_equals(std::string_view key, std::string_view val) const noexcept {
    if (auto i = find_by_key_icase(key); i != fields_.end())
      return val == i->second;
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

  /// Checks whether the client has defined `Transfer-Encoding` as `chunked`.
  bool chunked_transfer_encoding() const noexcept;

  /// Convenience function for `field_as<size_t>("Content-Length")`.
  std::optional<size_t> content_length() const noexcept;

  /// Checks if the request header is valid (non-empty).
  bool valid() const noexcept {
    return !raw_.empty();
  }

  /// Parses header fields from the provided data and returns the unprocessed
  /// input or error on invalid format.
  /// @note: Does not take ownership of the raw data.
  expected<std::string_view> parse_fields(std::string_view data);

protected:
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

  // Utility function to remap the string view from old base to new base.
  static std::string_view remap(const char* base, std::string_view src,
                                const char* new_base) noexcept {
    return std::string_view{new_base + std::distance(base, src.data()),
                            src.size()};
  };

  /// Reassigns the shallow map from one address to another.
  void reassign_fields(const header& other) noexcept;

  /// Stores the raw HTTP input.
  std::vector<char> raw_;

private:
  /// A shallow map for looking up individual header fields.
  fields_map fields_;
};

} // namespace caf::net::http
