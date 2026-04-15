// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/placement_ptr.hpp"
#include "caf/text_writer.hpp"

#include <cstddef>

namespace caf {

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer : public text_writer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  json_writer();

  explicit json_writer(actor_system& sys);

  ~json_writer() override;

  // -- properties -------------------------------------------------------------

  [[nodiscard]] std::string_view str() const noexcept final;

  [[nodiscard]] size_t indentation() const noexcept final;

  void indentation(size_t factor) noexcept final;

  [[nodiscard]] bool compact() const noexcept final;

  [[nodiscard]] bool skip_empty_fields() const noexcept final;

  void skip_empty_fields(bool value) noexcept final;

  [[nodiscard]] bool skip_object_type_annotation() const noexcept final;

  void skip_object_type_annotation(bool value) noexcept final;

  [[nodiscard]] std::string_view field_type_suffix() const noexcept final;

  void field_type_suffix(std::string_view suffix) noexcept final;

  [[nodiscard]] const type_id_mapper* mapper() const noexcept final;

  void mapper(const type_id_mapper* ptr) noexcept final;

  // -- modifiers --------------------------------------------------------------

  /// Removes all characters from the buffer and restores the writer to its
  /// initial state.
  /// @warning Invalidates all string views into the buffer.
  void reset() final;

  // -- finals --------------------------------------------------------------

  void set_error(error stop_reason) final;

  error& get_error() noexcept final;

  caf::actor_system* sys() const noexcept final;

  bool has_human_readable_format() const noexcept final;

  bool begin_object(type_id_t type, std::string_view name) final;

  bool end_object() final;

  bool begin_field(std::string_view) final;

  bool begin_field(std::string_view name, bool is_present) final;

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t index) final;

  bool begin_field(std::string_view name, bool is_present,
                   std::span<const type_id_t> types, size_t index) final;

  bool end_field() final;

  bool begin_tuple(size_t size) final;

  bool end_tuple() final;

  bool begin_key_value_pair() final;

  bool end_key_value_pair() final;

  bool begin_sequence(size_t size) final;

  bool end_sequence() final;

  bool begin_associative_array(size_t size) final;

  bool end_associative_array() final;

  using text_writer::value;

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
  static constexpr size_t impl_storage_size = 196;

  /// Opaque implementation class.
  class impl;

  /// Pointer to the implementation object.
  placement_ptr<impl> impl_;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
