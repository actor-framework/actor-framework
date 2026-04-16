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

  explicit json_writer(caf::actor_handle_codec* codec = nullptr);

  ~json_writer() noexcept override;

  json_writer(const json_writer&) = delete;

  json_writer& operator=(const json_writer&) = delete;

  [[nodiscard]] std::string_view str() const noexcept {
    return impl_->str();
  }

  [[nodiscard]] size_t indentation() const noexcept {
    return impl_->indentation();
  }

  void indentation(size_t factor) noexcept {
    impl_->indentation(factor);
  }

  [[nodiscard]] bool compact() const noexcept {
    return impl_->compact();
  }

  [[nodiscard]] bool skip_empty_fields() const noexcept {
    return impl_->skip_empty_fields();
  }

  void skip_empty_fields(bool value) noexcept {
    impl_->skip_empty_fields(value);
  }

  [[nodiscard]] bool skip_object_type_annotation() const noexcept {
    return impl_->skip_object_type_annotation();
  }

  void skip_object_type_annotation(bool value) noexcept {
    impl_->skip_object_type_annotation(value);
  }

  [[nodiscard]] std::string_view field_type_suffix() const noexcept {
    return impl_->field_type_suffix();
  }

  void field_type_suffix(std::string_view suffix) noexcept {
    impl_->field_type_suffix(suffix);
  }

  [[nodiscard]] const type_id_mapper* mapper() const noexcept {
    return impl_->mapper();
  }

  void mapper(const type_id_mapper* ptr) noexcept {
    impl_->mapper(ptr);
  }

  /// Removes all characters from the buffer and restores the writer to its
  /// initial state.
  /// @warning Invalidates all string views into the buffer.
  void reset() {
    impl_->reset();
  }

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return true;
  }

private:
  static constexpr size_t impl_storage_size = 196;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
