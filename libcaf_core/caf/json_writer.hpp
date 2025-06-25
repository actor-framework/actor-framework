// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/byte_writer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf {

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer : public byte_writer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  json_writer();

  explicit json_writer(actor_system& sys);

  ~json_writer() override;

  // -- properties -------------------------------------------------------------

  const_byte_span bytes() const final;

  /// Returns a string view into the internal buffer.
  /// @warning This view becomes invalid when calling any non-const member
  ///          function on the writer object.
  [[nodiscard]] std::string_view str() const noexcept;

  /// Returns the current indentation factor.
  [[nodiscard]] size_t indentation() const noexcept;

  /// Sets the indentation level.
  /// @param factor The number of spaces to add to each level of indentation. A
  ///               value of 0 (the default) disables indentation, printing the
  ///               entire JSON output into a single line.
  void indentation(size_t factor) noexcept;

  /// Returns whether the writer generates compact JSON output without any
  /// spaces or newlines to separate values.
  [[nodiscard]] bool compact() const noexcept;

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

  /// Returns the type ID mapper used by the writer.
  [[nodiscard]] const type_id_mapper* mapper() const noexcept;

  /// Changes the type ID mapper for the writer.
  void mapper(const type_id_mapper* ptr) noexcept;

  // -- modifiers --------------------------------------------------------------

  /// Removes all characters from the buffer and restores the writer to its
  /// initial state.
  /// @warning Invalidates all string views into the buffer.
  void reset() final;

  // -- overrides --------------------------------------------------------------

  void set_error(error stop_reason) final;

  error& get_error() noexcept final;

  caf::actor_system* sys() const noexcept final;

  bool has_human_readable_format() const noexcept final;

  bool begin_object(type_id_t type, std::string_view name) final;

  bool end_object() final;

  bool begin_field(std::string_view) final;

  bool begin_field(std::string_view name, bool is_present) final;

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t index) final;

  bool begin_field(std::string_view name, bool is_present,
                   span<const type_id_t> types, size_t index) final;

  bool end_field() final;

  bool begin_tuple(size_t size) final;

  bool end_tuple() final;

  bool begin_key_value_pair() final;

  bool end_key_value_pair() final;

  bool begin_sequence(size_t size) final;

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

  bool value(float x) final;

  bool value(double x) final;

  bool value(long double x) final;

  bool value(std::string_view x) final;

  bool value(const std::u16string& x) final;

  bool value(const std::u32string& x) final;

  bool value(const_byte_span x) final;

private:
  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_[256];
};

} // namespace caf
