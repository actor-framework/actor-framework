// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "caf/byte.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/fwd.hpp"
#include "caf/load_inspector_base.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent deserialization interface.
class CAF_CORE_EXPORT deserializer : public load_inspector_base<deserializer> {
public:
  // -- member types -----------------------------------------------------------

  using super = load_inspector_base<deserializer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit deserializer(actor_system& sys) noexcept;

  explicit deserializer(execution_unit* ctx = nullptr) noexcept;

  virtual ~deserializer();

  // -- properties -------------------------------------------------------------

  auto context() const noexcept {
    return context_;
  }

  bool has_human_readable_format() const noexcept {
    return has_human_readable_format_;
  }

  // -- interface functions ----------------------------------------------------

  /// Reads run-time-type information for the next object if available.
  virtual bool fetch_next_object_type(type_id_t& type) = 0;

  /// Reads run-time-type information for the next object if available. The
  /// default implementation calls `fetch_next_object_type` and queries the CAF
  /// type name. However, implementations of the interface may retrieve the type
  /// name differently and the type name may not correspond to any type known to
  /// CAF. For example. the @ref json_reader returns the content of the `@type`
  /// field of the current object if available.
  /// @warning the characters in `type_name` may point to an internal buffer
  ///          that becomes invalid as soon as calling *any* other member
  ///          function on the deserializer. Convert the `type_name` to a string
  ///          before storing it.
  virtual bool fetch_next_object_name(string_view& type_name);

  /// Convenience function for querying `fetch_next_object_name` comparing the
  /// result to `type_name` in one shot.
  bool next_object_name_matches(string_view type_name);

  /// Like `next_object_name_matches`, but sets an error on the deserializer
  /// on a mismatch.
  bool assert_next_object_name(string_view type_name);

  /// Begins processing of an object, may perform a type check depending on the
  /// data format.
  /// @param type 16-bit ID for known types, @ref invalid_type_id otherwise.
  /// @param pretty_class_name Either the output of @ref type_name_or_anonymous
  ///                          or the optionally defined pretty name.
  virtual bool begin_object(type_id_t type, string_view pretty_class_name) = 0;

  /// Ends processing of an object.
  virtual bool end_object() = 0;

  virtual bool begin_field(string_view name) = 0;

  virtual bool begin_field(string_view, bool& is_present) = 0;

  virtual bool
  begin_field(string_view name, span<const type_id_t> types, size_t& index)
    = 0;

  virtual bool begin_field(string_view name, bool& is_present,
                           span<const type_id_t> types, size_t& index)
    = 0;

  virtual bool end_field() = 0;

  /// Begins processing of a fixed-size sequence.
  virtual bool begin_tuple(size_t size) = 0;

  /// Ends processing of a sequence.
  virtual bool end_tuple() = 0;

  /// Begins processing of a tuple with two elements, whereas the first element
  /// represents the key in an associative array.
  /// @note the default implementation calls `begin_tuple(2)`.
  virtual bool begin_key_value_pair();

  /// Ends processing of a key-value pair after both values were written.
  /// @note the default implementation calls `end_tuple()`.
  virtual bool end_key_value_pair();

  /// Begins processing of a sequence.
  virtual bool begin_sequence(size_t& size) = 0;

  /// Ends processing of a sequence.
  virtual bool end_sequence() = 0;

  /// Begins processing of an associative array (map).
  /// @note the default implementation calls `begin_sequence(size)`.
  virtual bool begin_associative_array(size_t& size);

  /// Ends processing of an associative array (map).
  /// @note the default implementation calls `end_sequence()`.
  virtual bool end_associative_array();

  /// Reads `x` from the input.
  /// @param x A reference to a builtin type.
  /// @returns `true` on success, `false` otherwise.
  virtual bool value(byte& x) = 0;

  /// @copydoc value
  virtual bool value(bool& x) = 0;

  /// @copydoc value
  virtual bool value(int8_t&) = 0;

  /// @copydoc value
  virtual bool value(uint8_t&) = 0;

  /// @copydoc value
  virtual bool value(int16_t&) = 0;

  /// @copydoc value
  virtual bool value(uint16_t&) = 0;

  /// @copydoc value
  virtual bool value(int32_t&) = 0;

  /// @copydoc value
  virtual bool value(uint32_t&) = 0;

  /// @copydoc value
  virtual bool value(int64_t&) = 0;

  /// @copydoc value
  virtual bool value(uint64_t&) = 0;

  /// @copydoc value
  template <class T>
  std::enable_if_t<std::is_integral<T>::value, bool> value(T& x) noexcept {
    auto tmp = detail::squashed_int_t<T>{0};
    if (value(tmp)) {
      x = static_cast<T>(tmp);
      return true;
    } else {
      return false;
    }
  }
  /// @copydoc value
  virtual bool value(float&) = 0;

  /// @copydoc value
  virtual bool value(double&) = 0;

  /// @copydoc value
  virtual bool value(long double&) = 0;

  /// @copydoc value
  virtual bool value(std::string&) = 0;

  /// @copydoc value
  virtual bool value(std::u16string&) = 0;

  /// @copydoc value
  virtual bool value(std::u32string&) = 0;

  /// Reads a byte sequence from the input.
  /// @param x The byte sequence.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual bool value(span<byte> x) = 0;

  using super::list;

  /// Adds each boolean in `xs` to the output. Derived classes can override this
  /// member function to pack the booleans, for example to avoid using one byte
  /// for each value in a binary output format.
  virtual bool list(std::vector<bool>& xs);

protected:
  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;

  /// Configures whether client code should assume human-readable output.
  bool has_human_readable_format_ = false;
};

} // namespace caf
