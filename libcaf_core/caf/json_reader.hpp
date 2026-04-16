// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/load_inspector_base.hpp"
#include "caf/text_reader.hpp"

#include <cstddef>

namespace caf {

/// Deserializes an inspectable object from a JSON-formatted string.
class CAF_CORE_EXPORT json_reader final
  : public load_inspector_base<json_reader, text_reader> {
public:
  using super = load_inspector_base<json_reader, text_reader>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit json_reader(caf::actor_handle_codec* codec = nullptr);

  json_reader(const json_reader&) = delete;

  json_reader& operator=(const json_reader&) = delete;

  ~json_reader() noexcept override;

  // -- properties -------------------------------------------------------------

  [[nodiscard]] std::string_view field_type_suffix() const noexcept;

  void field_type_suffix(std::string_view suffix) noexcept;

  [[nodiscard]] const type_id_mapper* mapper() const noexcept;

  void mapper(const type_id_mapper* ptr) noexcept;

  /// Parses @p json_text into an internal representation. After loading the
  /// JSON input, the reader is ready for attempting to deserialize inspectable
  /// objects.
  /// @warning The internal data structure keeps pointers into @p json_text.
  ///          Hence, the buffer pointed to by the string view must remain valid
  ///          until either destroying this reader or calling `reset`.
  /// @note Implicitly calls `reset`.
  bool load(std::string_view json_text);

  bool load_bytes(const_byte_span bytes);

  /// Reads the input stream @p input and parses the content into an internal
  /// representation. After loading the JSON input, the reader is ready for
  /// attempting to deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_from(std::istream& input);

  /// Parses the content of the file under the given @p path. After loading the
  /// content of the JSON file, the reader is ready for attempting to
  /// deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_file(const char* path);

  /// @copydoc load_file
  bool load_file(const std::string& path);

  /// Reverts the state of the reader back to where it was after calling `load`.
  /// @post The reader is ready for attempting to deserialize another
  ///       inspectable object.
  void revert();

  /// Removes any loaded JSON data and reclaims memory resources.
  void reset();

  bool fetch_next_object_name(std::string_view& type_name) {
    return impl_->fetch_next_object_name(type_name);
  }

  bool next_object_name_matches(std::string_view type_name) {
    return impl_->next_object_name_matches(type_name);
  }

  bool assert_next_object_name(std::string_view type_name) {
    return impl_->assert_next_object_name(type_name);
  }

  bool has_human_readable_format() const noexcept {
    return true;
  }

private:
  static constexpr size_t impl_storage_size = 196;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
