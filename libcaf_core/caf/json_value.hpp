// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/json.hpp"
#include "caf/fwd.hpp"
#include "caf/make_counted.hpp"

#include <string>
#include <string_view>

namespace caf {

/// Represents an immutable JSON value.
class CAF_CORE_EXPORT json_value {
public:
  // -- constructors, destructors, and assignment operators --------------------

  /// Creates a @c null JSON value.
  json_value() noexcept;

  json_value(const detail::json::value* val,
             detail::json::storage_ptr sptr) noexcept
    : val_(val), storage_(sptr) {
    // nop
  }

  json_value(json_value&&) noexcept = default;

  json_value(const json_value&) noexcept = default;

  json_value& operator=(json_value&&) noexcept = default;

  json_value& operator=(const json_value&) noexcept = default;

  // -- factories --------------------------------------------------------------

  /// Creates an undefined JSON value. This special state usually indicates that
  /// a previous key lookup failed.
  static json_value undefined() noexcept;

  // -- properties -------------------------------------------------------------

  /// Checks whether the value is @c null.
  bool is_null() const noexcept {
    return val_->is_null();
  }

  /// Checks whether the value is undefined. This special state indicates that a
  /// previous key lookup failed.
  bool is_undefined() const noexcept {
    return val_->is_undefined();
  }

  /// Checks whether the value is an @c int64_t.
  bool is_integer() const noexcept;

  /// Checks whether the value is an @c uint64_t.
  bool is_unsigned() const noexcept;

  /// Checks whether the value is a @c double.
  bool is_double() const noexcept {
    return val_->is_double();
  }

  /// Checks whether the value is a number, i.e., an @c int64_t, a @c uint64_t
  /// or a @c double.
  bool is_number() const noexcept;

  /// Checks whether the value is a @c bool.
  bool is_bool() const noexcept {
    return val_->is_bool();
  }

  /// Checks whether the value is a JSON string (@c std::string_view).
  bool is_string() const noexcept {
    return val_->is_string();
  }

  /// Checks whether the value is an JSON array.
  bool is_array() const noexcept {
    return val_->is_array();
  }

  /// Checks whether the value is a JSON object.
  bool is_object() const noexcept {
    return val_->is_object();
  }

  // -- conversion -------------------------------------------------------------

  /// Converts the value to an @c int64_t or returns @p fallback if the value
  /// is not a valid number.
  int64_t to_integer(int64_t fallback = 0) const;

  /// Converts the value to an @c uint64_t or returns @p fallback if the value
  /// is not a valid number.
  uint64_t to_unsigned(uint64_t fallback = 0) const;

  /// Converts the value to a @c double or returns @p fallback if the value is
  /// not a valid number.
  double to_double(double fallback = 0.0) const;

  /// Converts the value to a @c bool or returns @p fallback if the value is
  /// not a valid boolean.
  bool to_bool(bool fallback = false) const;

  /// Returns the value as a JSON string or an empty string if the value is not
  /// a string.
  std::string_view to_string() const;

  /// Returns the value as a JSON string or @p fallback if the value is not a
  /// string.
  std::string_view to_string(std::string_view fallback) const;

  /// Returns the value as a JSON array or an empty array if the value is not
  /// an array.
  json_array to_array() const;

  /// Returns the value as a JSON array or @p fallback if the value is not an
  /// array.
  json_array to_array(json_array fallback) const;

  /// Returns the value as a JSON object or an empty object if the value is not
  /// an object.
  json_object to_object() const;

  /// Returns the value as a JSON object or @p fallback if the value is not an
  /// object.
  json_object to_object(json_object fallback) const;

  // -- comparison -------------------------------------------------------------

  /// Compares two JSON values for equality.
  bool equal_to(const json_value& other) const noexcept;

  // -- parsing ----------------------------------------------------------------

  /// Attempts to parse @p str as JSON input into a self-contained value.
  static expected<json_value> parse(std::string_view str);

  /// Attempts to parse @p str as JSON input into a value that avoids copies
  /// where possible by pointing into @p str.
  /// @warning The returned @ref json_value may hold pointers into @p str. Thus,
  ///          the input *must* outlive the @ref json_value and any other JSON
  ///          objects created from that value.
  static expected<json_value> parse_shallow(std::string_view str);

  /// Attempts to parse @p str as JSON input. Decodes JSON in place and points
  /// back into the @p str for all strings in the JSON input.
  /// @warning The returned @ref json_value may hold pointers into @p str. Thus,
  ///          the input *must* outlive the @ref json_value and any other JSON
  ///          objects created from that value.
  static expected<json_value> parse_in_situ(std::string& str);

  /// Attempts to parse the content of the file at @p path as JSON input into a
  /// self-contained value.
  static expected<json_value> parse_file(const char* path);

  /// @copydoc parse_file
  static expected<json_value> parse_file(const std::string& path);

  // -- printing ---------------------------------------------------------------

  /// Prints the JSON value to @p buf.
  template <class Buffer>
  void print_to(Buffer& buf, size_t indentation_factor = 0) const {
    detail::json::print_to(buf, *val_, indentation_factor);
  }

  // -- serialization ----------------------------------------------------------

  /// Applies the `Inspector` to the JSON value `val`.
  template <class Inspector>
  friend bool inspect(Inspector& inspector, json_value& val) {
    if constexpr (Inspector::is_loading) {
      auto storage = make_counted<detail::json::storage>();
      auto* internal_val = detail::json::make_value(storage);
      if (!detail::json::load(inspector, *internal_val, storage))
        return false;
      val = json_value{internal_val, std::move(storage)};
      return true;
    } else {
      return detail::json::save(inspector, *val.val_);
    }
  }

private:
  const detail::json::value* val_;
  detail::json::storage_ptr storage_;
};

// -- free functions -----------------------------------------------------------

inline bool operator==(const json_value& lhs, const json_value& rhs) {
  return lhs.equal_to(rhs);
}

inline bool operator!=(const json_value& lhs, const json_value& rhs) {
  return !(lhs == rhs);
}

/// @relates json_value
CAF_CORE_EXPORT std::string to_string(const json_value& val);

} // namespace caf
