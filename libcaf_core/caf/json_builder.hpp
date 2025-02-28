// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/serializer.hpp"

#include <cstddef>

namespace caf {

/// Serializes an inspectable object to a @ref json_value.
class CAF_CORE_EXPORT json_builder final : public serializer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  json_builder();

  json_builder(const json_builder&) = delete;

  json_builder& operator=(const json_builder&) = delete;

  ~json_builder() override;

  // -- properties -------------------------------------------------------------

  /// Returns whether the writer omits empty fields entirely (true) or renders
  /// empty fields as `$field: null` (false).
  [[nodiscard]] bool skip_empty_fields() const noexcept;

  /// Configures whether the writer omits empty fields.
  void skip_empty_fields(bool value) noexcept;

  /// Returns whether the writer omits `@type` annotations for JSON objects.
  [[nodiscard]] bool skip_object_type_annotation() const noexcept;

  /// Configures whether the writer omits `@type` annotations for JSON objects.
  void skip_object_type_annotation(bool value) noexcept;

  /// Returns the suffix for generating type annotation fields for variant
  /// fields. For example, CAF inserts field called "@foo${field_type_suffix}"
  /// for a variant field called "foo".
  [[nodiscard]] std::string_view field_type_suffix() const noexcept;

  /// Configures whether the writer omits empty fields.
  void field_type_suffix(std::string_view suffix) noexcept;

  // -- modifiers --------------------------------------------------------------

  /// Restores the writer to its initial state.
  void reset();

  /// Seals the JSON value, i.e., rendering it immutable, and returns it. After
  /// calling this member function, the @ref json_builder is in a moved-from
  /// state and users may only call @c reset to start a new building process or
  /// destroy this instance.
  json_value seal();

  // -- overrides --------------------------------------------------------------

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
  alignas(std::max_align_t) std::byte impl_[96];
};

} // namespace caf
