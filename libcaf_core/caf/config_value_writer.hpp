// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/serializer.hpp"

#include <cstddef>

namespace caf {

/// Serializes an objects into a @ref config_value.
class CAF_CORE_EXPORT config_value_writer final : public serializer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit config_value_writer(config_value* dst);

  config_value_writer(config_value* dst, actor_system& sys);

  ~config_value_writer() override;

  // -- interface functions ----------------------------------------------------

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  caf::actor_system* sys() const noexcept override;

  bool has_human_readable_format() const noexcept override;

  bool begin_object(type_id_t type, std::string_view name) override;

  bool end_object() override;

  bool begin_field(std::string_view) override;

  bool begin_field(std::string_view name, bool is_present) override;

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(std::string_view name, bool is_present,
                   span<const type_id_t> types, size_t index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t size) override;

  bool end_associative_array() override;

  bool value(std::byte x) override;

  bool value(bool x) override;

  bool value(int8_t x) override;

  bool value(uint8_t x) override;

  bool value(int16_t x) override;

  bool value(uint16_t x) override;

  bool value(int32_t x) override;

  bool value(uint32_t x) override;

  bool value(int64_t x) override;

  bool value(uint64_t x) override;

  bool value(float x) override;

  bool value(double x) override;

  bool value(long double x) override;

  bool value(std::string_view x) override;

  bool value(const std::u16string& x) override;

  bool value(const std::u32string& x) override;

  bool value(const_byte_span x) override;

private:
  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_[64];
};

} // namespace caf
