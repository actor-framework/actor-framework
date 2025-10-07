// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/load_inspector_base.hpp"

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

  explicit binary_deserializer(const_byte_span input) noexcept;

  binary_deserializer(actor_system& sys, const_byte_span input) noexcept;

  binary_deserializer(const void* buf, size_t size) noexcept;

  binary_deserializer(actor_system& sys, const void* buf, size_t size) noexcept;

  ~binary_deserializer() override;

  binary_deserializer(const binary_deserializer&) = delete;

  binary_deserializer& operator=(const binary_deserializer&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns how many bytes are still available to read.
  size_t remaining() const noexcept;

  /// Returns the remaining bytes.
  const_byte_span remainder() const noexcept;

  /// Returns the current execution unit.
  actor_system* context() const noexcept;

  /// Jumps `num_bytes` forward.
  /// @pre `num_bytes <= remaining()`
  void skip(size_t num_bytes);

  /// Assigns a new input.
  void reset(const_byte_span bytes) noexcept;

  /// Returns the current read position.
  const std::byte* current() const noexcept;

  /// Returns the end of the assigned memory block.
  const std::byte* end() const noexcept;

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  // -- overridden member functions --------------------------------------------

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool fetch_next_object_type(type_id_t& type) noexcept;

  bool begin_object(type_id_t, std::string_view) noexcept;

  bool end_object() noexcept;

  bool begin_field(std::string_view) noexcept;

  bool begin_field(std::string_view name, bool& is_present) noexcept;

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t& index) noexcept;

  bool begin_field(std::string_view name, bool& is_present,
                   std::span<const type_id_t> types, size_t& index) noexcept;

  bool end_field();

  bool begin_tuple(size_t) noexcept;

  bool end_tuple() noexcept;

  bool begin_key_value_pair() noexcept;

  bool end_key_value_pair() noexcept;

  bool begin_sequence(size_t& list_size) noexcept;

  bool end_sequence() noexcept;

  bool begin_associative_array(size_t& size) noexcept;

  bool end_associative_array() noexcept;

  bool value(bool& x) noexcept;

  bool value(std::byte& x) noexcept;

  bool value(uint8_t& x) noexcept;

  bool value(int8_t& x) noexcept;

  bool value(int16_t& x) noexcept;

  bool value(uint16_t& x) noexcept;

  bool value(int32_t& x) noexcept;

  bool value(uint32_t& x) noexcept;

  bool value(int64_t& x) noexcept;

  bool value(uint64_t& x) noexcept;

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

  bool value(float& x) noexcept;

  bool value(double& x) noexcept;

  bool value(long double& x);

  bool value(std::string& x);

  bool value(std::u16string& x);

  bool value(std::u32string& x);

  bool value(byte_span x) noexcept;

  bool value(std::vector<bool>& x);

  bool value(strong_actor_ptr& ptr);

  bool value(weak_actor_ptr& ptr);

private:
  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_[48];
};

} // namespace caf
