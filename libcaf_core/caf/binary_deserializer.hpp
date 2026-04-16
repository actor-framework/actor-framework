// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_reader.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/placement_ptr.hpp"

#include <concepts>
#include <cstddef>
#include <span>

namespace caf {

/// Deserializes C++ objects from sequence of bytes. Does not perform
/// run-time type checks.
class CAF_CORE_EXPORT binary_deserializer final
  : public load_inspector_base<binary_deserializer> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit binary_deserializer(const_byte_span input,
                               caf::actor_handle_codec* codec
                               = nullptr) noexcept;

  binary_deserializer(const void* buf, size_t size,
                      caf::actor_handle_codec* codec = nullptr) noexcept;

  ~binary_deserializer() noexcept override;

  binary_deserializer(const binary_deserializer&) = delete;

  binary_deserializer& operator=(const binary_deserializer&) = delete;

  // -- byte_reader overrides --------------------------------------------------

  [[nodiscard]] bool load_bytes(const_byte_span bytes) {
    return impl_->load_bytes(bytes);
  }

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return false;
  }

  // -- overridden member functions --------------------------------------------

  void set_error(error stop_reason) override;

  error& get_error() noexcept final;

  bool fetch_next_object_type(type_id_t& type) noexcept {
    return impl_->fetch_next_object_type(type);
  }

  bool begin_object(type_id_t type, std::string_view name) noexcept {
    return impl_->begin_object(type, name);
  }

  bool end_object() noexcept {
    return impl_->end_object();
  }

  bool begin_field(std::string_view name) noexcept {
    return impl_->begin_field(name);
  }

  bool begin_field(std::string_view name, bool& is_present) noexcept {
    return impl_->begin_field(name, is_present);
  }

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t& index) noexcept {
    return impl_->begin_field(name, types, index);
  }

  bool begin_field(std::string_view name, bool& is_present,
                   std::span<const type_id_t> types, size_t& index) noexcept {
    return impl_->begin_field(name, is_present, types, index);
  }

  bool end_field() {
    return impl_->end_field();
  }

  bool begin_tuple(size_t size) noexcept {
    return impl_->begin_tuple(size);
  }

  bool end_tuple() noexcept {
    return impl_->end_tuple();
  }

  bool begin_key_value_pair() noexcept {
    return impl_->begin_key_value_pair();
  }

  bool end_key_value_pair() noexcept {
    return impl_->end_key_value_pair();
  }

  bool begin_sequence(size_t& list_size) noexcept {
    return impl_->begin_sequence(list_size);
  }

  bool end_sequence() noexcept {
    return impl_->end_sequence();
  }

  bool begin_associative_array(size_t& size) noexcept {
    return impl_->begin_associative_array(size);
  }

  bool end_associative_array() noexcept {
    return impl_->end_associative_array();
  }

  bool value(bool& what) noexcept {
    return impl_->value(what);
  }

  bool value(std::byte& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint8_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int8_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int16_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint16_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int32_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint32_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int64_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint64_t& what) noexcept {
    return impl_->value(what);
  }

  template <std::integral T>
  bool value(T& what) noexcept {
    auto tmp = detail::squashed_int_t<T>{0};
    if (impl_->value(tmp)) {
      what = static_cast<T>(tmp);
      return true;
    } else {
      return false;
    }
  }

  template <std::floating_point T>
  bool value(T& what) noexcept {
    return impl_->value(what);
  }

  bool value(strong_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(weak_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(std::string& what) {
    return impl_->value(what);
  }

  bool value(std::u16string& what) {
    return impl_->value(what);
  }

  bool value(std::u32string& what) {
    return impl_->value(what);
  }

  bool value(byte_span what) noexcept {
    return impl_->value(what);
  }

  bool value(std::vector<bool>& what);

  caf::actor_handle_codec* actor_handle_codec() {
    return impl_->actor_handle_codec();
  }

  byte_reader& as_deserializer() noexcept {
    return *impl_;
  }

private:
  static constexpr size_t impl_storage_size = 48;

  /// Pointer to the implementation object.
  placement_ptr<byte_reader> impl_;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
