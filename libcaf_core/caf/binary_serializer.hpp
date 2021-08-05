// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/byte_buffer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/span.hpp"

namespace caf {

/// Serializes C++ objects into a sequence of bytes.
/// @note The binary data format may change between CAF versions and does not
///       perform any type checking at run-time. Thus the output of this
///       serializer is unsuitable for persistence layers.
class CAF_CORE_EXPORT binary_serializer
  : public save_inspector_base<binary_serializer> {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector_base<binary_serializer>;

  using container_type = byte_buffer;

  using value_type = std::byte;

  // -- constructors, destructors, and assignment operators --------------------

  binary_serializer(actor_system& sys, byte_buffer& buf) noexcept;

  binary_serializer(execution_unit* ctx, byte_buffer& buf) noexcept
    : buf_(buf), write_pos_(buf.size()), context_(ctx) {
    // nop
  }

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the current execution unit.
  execution_unit* context() const noexcept {
    return context_;
  }

  byte_buffer& buf() noexcept {
    return buf_;
  }

  const byte_buffer& buf() const noexcept {
    return buf_;
  }

  size_t write_pos() const noexcept {
    return write_pos_;
  }

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  // -- position management ----------------------------------------------------

  /// Sets the write position to `offset`.
  /// @pre `offset <= buf.size()`
  void seek(size_t offset) noexcept {
    write_pos_ = offset;
  }

  /// Jumps `num_bytes` forward. Resizes the buffer (filling it with zeros)
  /// when skipping past the end.
  void skip(size_t num_bytes);

  // -- interface functions ----------------------------------------------------

  constexpr bool begin_object(type_id_t, std::string_view) noexcept {
    return true;
  }

  constexpr bool end_object() {
    return true;
  }

  constexpr bool begin_field(std::string_view) noexcept {
    return true;
  }

  bool begin_field(std::string_view, bool is_present);

  bool begin_field(std::string_view, span<const type_id_t> types, size_t index);

  bool begin_field(std::string_view, bool is_present,
                   span<const type_id_t> types, size_t index);

  constexpr bool end_field() {
    return true;
  }

  constexpr bool begin_tuple(size_t) {
    return true;
  }

  constexpr bool end_tuple() {
    return true;
  }

  constexpr bool begin_key_value_pair() {
    return true;
  }

  constexpr bool end_key_value_pair() {
    return true;
  }

  bool begin_sequence(size_t list_size);

  constexpr bool end_sequence() {
    return true;
  }

  bool begin_associative_array(size_t size) {
    return begin_sequence(size);
  }

  bool end_associative_array() {
    return end_sequence();
  }

  bool value(std::byte x);

  bool value(bool x);

  bool value(int8_t x);

  bool value(uint8_t x);

  bool value(int16_t x);

  bool value(uint16_t x);

  bool value(int32_t x);

  bool value(uint32_t x);

  bool value(int64_t x);

  bool value(uint64_t x);

  template <class T>
  std::enable_if_t<std::is_integral<T>::value, bool> value(T x) {
    return value(static_cast<detail::squashed_int_t<T>>(x));
  }

  bool value(float x);

  bool value(double x);

  bool value(long double x);

  bool value(std::string_view x);

  bool value(const std::u16string& x);

  bool value(const std::u32string& x);

  bool value(span<const std::byte> x);

  bool value(const std::vector<bool>& x);

private:
  /// Stores the serialized output.
  byte_buffer& buf_;

  /// Stores the current offset for writing.
  size_t write_pos_;

  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;
};

} // namespace caf
