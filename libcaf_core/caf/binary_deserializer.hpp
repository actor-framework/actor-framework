// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

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
class CAF_CORE_EXPORT binary_deserializer final : public byte_reader {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit binary_deserializer(const_byte_span input) noexcept;

  binary_deserializer(actor_system& sys, const_byte_span input) noexcept;

  binary_deserializer(const void* buf, size_t size) noexcept;

  binary_deserializer(actor_system& sys, const void* buf, size_t size) noexcept;

  ~binary_deserializer() override;

  binary_deserializer(const binary_deserializer&) = delete;

  binary_deserializer& operator=(const binary_deserializer&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the current execution unit.
  actor_system* context() const noexcept;

  // -- byte_reader overrides --------------------------------------------------

  [[nodiscard]] bool load_bytes(const_byte_span bytes) final;

  [[nodiscard]] caf::actor_system* sys() const noexcept final;

  [[nodiscard]] bool has_human_readable_format() const noexcept final;

  // -- overridden member functions --------------------------------------------

  void set_error(error stop_reason) final;

  error& get_error() noexcept final;

  bool fetch_next_object_type(type_id_t& type) noexcept final;

  bool begin_object(type_id_t, std::string_view) noexcept final;

  bool end_object() noexcept final;

  bool begin_field(std::string_view) noexcept final;

  bool begin_field(std::string_view name, bool& is_present) noexcept final;

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t& index) noexcept final;

  bool begin_field(std::string_view name, bool& is_present,
                   std::span<const type_id_t> types,
                   size_t& index) noexcept final;

  bool end_field() final;

  bool begin_tuple(size_t) noexcept final;

  bool end_tuple() noexcept final;

  bool begin_key_value_pair() noexcept final;

  bool end_key_value_pair() noexcept final;

  bool begin_sequence(size_t& list_size) noexcept final;

  bool end_sequence() noexcept final;

  bool begin_associative_array(size_t& size) noexcept final;

  bool end_associative_array() noexcept final;

  bool value(bool& x) noexcept final;

  bool value(std::byte& x) noexcept final;

  bool value(uint8_t& x) noexcept final;

  bool value(int8_t& x) noexcept final;

  bool value(int16_t& x) noexcept final;

  bool value(uint16_t& x) noexcept final;

  bool value(int32_t& x) noexcept final;

  bool value(uint32_t& x) noexcept final;

  bool value(int64_t& x) noexcept final;

  bool value(uint64_t& x) noexcept final;

  template <std::integral T>
  bool value(T& x) noexcept {
    auto tmp = detail::squashed_int_t<T>{0};
    if (value(tmp)) {
      x = static_cast<T>(tmp);
      return true;
    } else {
      return false;
    }
  }

  bool value(float& x) noexcept final;

  bool value(double& x) noexcept final;

  bool value(long double& x) final;

  bool value(std::string& x) final;

  bool value(std::u16string& x) final;

  bool value(std::u32string& x) final;

  bool value(byte_span x) noexcept final;

  bool value(std::vector<bool>& x);

  bool value(strong_actor_ptr& ptr) final;

  bool value(weak_actor_ptr& ptr) final;

private:
  static constexpr size_t impl_storage_size = 48;

  /// Opaque implementation class.
  class impl;

  /// Pointer to the implementation object.
  placement_ptr<impl> impl_;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
