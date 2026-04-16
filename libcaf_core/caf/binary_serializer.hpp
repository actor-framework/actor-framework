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
class CAF_CORE_EXPORT binary_serializer final
  : public save_inspector_base<binary_serializer> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit binary_serializer(byte_buffer& buf,
                             caf::actor_handle_codec* codec = nullptr) noexcept;

  ~binary_serializer() noexcept override;

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  // -- properties -------------------------------------------------------------

  void reset() {
    impl_->reset();
  }

  // -- byte_writer overrides --------------------------------------------------

  [[nodiscard]] const_byte_span bytes() const noexcept {
    return impl_->bytes();
  }

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return false;
  }

  // -- position management ----------------------------------------------------

  /// Jumps `num_bytes` forward by inserting `num_bytes` zeros at the end of the
  /// buffer.
  /// @returns the offset where the zero-bytes were inserted.
  [[nodiscard]] size_t skip(size_t num_bytes) {
    return impl_->skip(num_bytes);
  }

  /// Overrides the buffer at `offset` with `content`.
  /// @returns `true` if the buffer was large enough to hold `content`, `false`
  ///          otherwise.
  [[nodiscard]] bool update(size_t offset, const_byte_span content) noexcept {
    return impl_->update(offset, content);
  }

  // -- interface functions ----------------------------------------------------

  void set_error(error stop_reason) override;

  error& get_error() noexcept final;

  bool begin_object(type_id_t id, std::string_view name) noexcept {
    return impl_->begin_object(id, name);
  }

  bool end_object() {
    return impl_->end_object();
  }

  bool begin_field(std::string_view type_name) noexcept {
    return impl_->begin_field(type_name);
  }

  bool begin_field(std::string_view type_name, bool is_present) {
    return impl_->begin_field(type_name, is_present);
  }

  bool begin_field(std::string_view type_name, std::span<const type_id_t> types,
                   size_t index) {
    return impl_->begin_field(type_name, types, index);
  }

  bool begin_field(std::string_view type_name, bool is_present,
                   std::span<const type_id_t> types, size_t index) {
    return impl_->begin_field(type_name, is_present, types, index);
  }

  bool end_field() {
    return impl_->end_field();
  }

  bool begin_tuple(size_t size) {
    return impl_->begin_tuple(size);
  }

  bool end_tuple() {
    return impl_->end_tuple();
  }

  bool begin_key_value_pair() {
    return impl_->begin_key_value_pair();
  }

  bool end_key_value_pair() {
    return impl_->end_key_value_pair();
  }

  bool begin_sequence(size_t list_size) {
    return impl_->begin_sequence(list_size);
  }

  bool end_sequence() {
    return impl_->end_sequence();
  }

  bool begin_associative_array(size_t size) {
    return impl_->begin_associative_array(size);
  }

  bool end_associative_array() {
    return impl_->end_associative_array();
  }

  bool value(std::byte what) {
    return impl_->value(what);
  }

  template <std::integral T>
  bool value(T what) {
    return impl_->value(static_cast<detail::squashed_int_t<T>>(what));
  }

  template <std::floating_point T>
  bool value(T what) {
    return impl_->value(what);
  }

  bool value(const strong_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(const weak_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(std::string_view what) {
    return impl_->value(what);
  }

  bool value(const std::u16string& what) {
    return impl_->value(what);
  }

  bool value(const std::u32string& what) {
    return impl_->value(what);
  }

  bool value(const_byte_span what) {
    return impl_->value(what);
  }

  bool value(const std::vector<bool>& what);

  caf::actor_handle_codec* actor_handle_codec() {
    return impl_->actor_handle_codec();
  }

  byte_writer& as_serializer() noexcept {
    return *impl_;
  }

private:
  static constexpr size_t impl_storage_size = 40;

  /// Pointer to the implementation object.
  placement_ptr<byte_writer> impl_;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
