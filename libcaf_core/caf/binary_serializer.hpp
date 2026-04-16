// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_writer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/placement_ptr.hpp"

#include <concepts>
#include <cstddef>

namespace caf {

/// Serializes C++ objects into a sequence of bytes.
/// @note The binary data format may change between CAF versions and does not
///       perform any type checking at run-time. Thus the output of this
///       serializer is unsuitable for persistence layers.
class CAF_CORE_EXPORT binary_serializer final : public byte_writer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit binary_serializer(byte_buffer& buf,
                             caf::actor_handle_codec* codec = nullptr) noexcept;

  ~binary_serializer() override;

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  // -- properties -------------------------------------------------------------

  void reset() final;

  // -- byte_writer overrides --------------------------------------------------

  [[nodiscard]] const_byte_span bytes() const noexcept final;

  [[nodiscard]] bool has_human_readable_format() const noexcept final;

  // -- position management ----------------------------------------------------

  /// Jumps `num_bytes` forward by inserting `num_bytes` zeros at the end of the
  /// buffer.
  /// @returns the offset where the zero-bytes were inserted.
  [[nodiscard]] size_t skip(size_t num_bytes) final;

  /// Overrides the buffer at `offset` with `content`.
  /// @returns `true` if the buffer was large enough to hold `content`, `false`
  ///          otherwise.
  [[nodiscard]] bool update(size_t offset,
                            const_byte_span content) noexcept final;

  // -- interface functions ----------------------------------------------------

  void set_error(error stop_reason) final;

  error& get_error() noexcept final;

  bool begin_object(type_id_t, std::string_view) noexcept final;

  bool end_object() final;

  bool begin_field(std::string_view) noexcept final;

  bool begin_field(std::string_view, bool is_present) final;

  bool begin_field(std::string_view, std::span<const type_id_t> types,
                   size_t index) final;

  bool begin_field(std::string_view, bool is_present,
                   std::span<const type_id_t> types, size_t index) final;

  bool end_field() final;

  bool begin_tuple(size_t) final;

  bool end_tuple() final;

  bool begin_key_value_pair() final;

  bool end_key_value_pair() final;

  bool begin_sequence(size_t list_size) final;

  bool end_sequence() final;

  bool begin_associative_array(size_t size) final;

  bool end_associative_array() final;

  using byte_writer::value;

  bool value(std::byte x) final;

  bool value(bool x) final;

  bool value(int8_t x) final;

  bool value(uint8_t x) final;

  bool value(int16_t x) final;

  bool value(uint16_t x) final;

  bool value(int32_t x) final;

  bool value(uint32_t x) final;

  bool value(int64_t x) final;

  bool value(uint64_t x) final;

  template <std::integral T>
  bool value(T x) {
    return value(static_cast<detail::squashed_int_t<T>>(x));
  }

  bool value(float x) final;

  bool value(double x) final;

  bool value(long double x) final;

  bool value(std::string_view x) final;

  bool value(const std::u16string& x) final;

  bool value(const std::u32string& x) final;

  bool value(const_byte_span x) final;

  bool value(const std::vector<bool>& x);

  caf::actor_handle_codec* actor_handle_codec() final;

private:
  static constexpr size_t impl_storage_size = 40;

  /// Opaque implementation class.
  class impl;

  /// Pointer to the implementation object.
  placement_ptr<impl> impl_;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
