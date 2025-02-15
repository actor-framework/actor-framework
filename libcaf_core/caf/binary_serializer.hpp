// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"

#include <cstddef>

namespace caf {

/// Serializes C++ objects into a sequence of bytes.
/// @note The binary data format may change between CAF versions and does not
///       perform any type checking at run-time. Thus the output of this
///       serializer is unsuitable for persistence layers.
class CAF_CORE_EXPORT binary_serializer final
  : public save_inspector_base<binary_serializer> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit binary_serializer(byte_buffer& buf) noexcept;

  binary_serializer(actor_system& sys, byte_buffer& buf) noexcept;

  [[deprecated("use the single-argument constructor instead")]] //
  binary_serializer(std::nullptr_t, byte_buffer& buf) noexcept;

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the current execution unit.
  actor_system* context() const noexcept;

  byte_buffer& buf() noexcept;

  const byte_buffer& buf() const noexcept;

  size_t write_pos() const noexcept;

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  // -- position management ----------------------------------------------------

  /// Sets the write position to `offset`.
  /// @pre `offset <= buf.size()`
  void seek(size_t offset) noexcept;

  /// Jumps `num_bytes` forward. Resizes the buffer (filling it with zeros)
  /// when skipping past the end.
  void skip(size_t num_bytes);

  // -- interface functions ----------------------------------------------------

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool begin_object(type_id_t, std::string_view) noexcept;

  bool end_object();

  bool begin_field(std::string_view) noexcept;

  bool begin_field(std::string_view, bool is_present);

  bool begin_field(std::string_view, span<const type_id_t> types, size_t index);

  bool begin_field(std::string_view, bool is_present,
                   span<const type_id_t> types, size_t index);

  bool end_field();

  bool begin_tuple(size_t);

  bool end_tuple();

  bool begin_key_value_pair();

  bool end_key_value_pair();

  bool begin_sequence(size_t list_size);

  bool end_sequence();

  bool begin_associative_array(size_t size);

  bool end_associative_array();

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
  std::enable_if_t<std::is_integral_v<T>, bool> value(T x) {
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

  virtual bool value(const strong_actor_ptr& ptr);

  virtual bool value(const weak_actor_ptr& ptr);

private:
  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_[40];
};

} // namespace caf
