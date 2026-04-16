// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/serializer.hpp"

#include <cstddef>
#include <string_view>

namespace caf {

/// Serializes an inspectable object to a @ref json_value.
class CAF_CORE_EXPORT json_builder final
  : public save_inspector_base<json_builder, serializer> {
public:
  using super = save_inspector_base<json_builder, serializer>;

  explicit json_builder(caf::actor_handle_codec* codec = nullptr);

  json_builder(const json_builder&) = delete;

  json_builder& operator=(const json_builder&) = delete;

  ~json_builder() noexcept override;

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

private:
  static constexpr size_t impl_storage_size = 96;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
