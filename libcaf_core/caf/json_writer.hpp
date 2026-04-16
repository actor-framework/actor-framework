// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/text_writer.hpp"

#include <cstddef>

namespace caf {

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer final
  : public save_inspector_base<json_writer, text_writer> {
public:
  using super = save_inspector_base<json_writer, text_writer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit json_writer(caf::actor_handle_codec* codec = nullptr);

  ~json_writer() noexcept override;

  json_writer(const json_writer&) = delete;

  json_writer& operator=(const json_writer&) = delete;

  // -- properties -------------------------------------------------------------

  [[nodiscard]] std::string_view str() const noexcept;

  [[nodiscard]] size_t indentation() const noexcept;

  void indentation(size_t factor) noexcept;

  [[nodiscard]] bool compact() const noexcept;

  [[nodiscard]] bool skip_empty_fields() const noexcept;

  void skip_empty_fields(bool value) noexcept;

  [[nodiscard]] bool skip_object_type_annotation() const noexcept;

  void skip_object_type_annotation(bool value) noexcept;

  [[nodiscard]] std::string_view field_type_suffix() const noexcept;

  void field_type_suffix(std::string_view suffix) noexcept;

  [[nodiscard]] const type_id_mapper* mapper() const noexcept;

  void mapper(const type_id_mapper* ptr) noexcept;

  // -- modifiers --------------------------------------------------------------

  /// Removes all characters from the buffer and restores the writer to its
  /// initial state.
  /// @warning Invalidates all string views into the buffer.
  void reset();

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return true;
  }

private:
  static constexpr size_t impl_storage_size = 196;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
