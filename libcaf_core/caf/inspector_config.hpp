// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

namespace caf {

/// Specifies how an inspector encodes object types for inspectors that support
/// both type IDs and type names.
enum class object_type_encoding {
  /// Leaves the encoding to the inspector implementation.
  default_mode,
  /// Encodes object types using type IDs.
  type_id,
  /// Encodes object types using type names.
  type_name,
};

/// Configures a serializer or deserializer.
class inspector_config {
public:
  /// Returns the configured actor handle codec.
  [[nodiscard]] constexpr actor_handle_codec* codec() const noexcept {
    return codec_;
  }

  /// Changes the configured actor handle codec.
  constexpr inspector_config& codec(actor_handle_codec* ptr) noexcept {
    codec_ = ptr;
    return *this;
  }

  /// Returns the configured type ID mapper.
  [[nodiscard]] constexpr const type_id_mapper& mapper() const noexcept {
    return mapper_ ? *mapper_ : get_default_type_id_mapper();
  }

  /// Changes the configured type ID mapper.
  constexpr inspector_config& mapper(const type_id_mapper* ptr) noexcept {
    mapper_ = ptr;
    return *this;
  }

  /// Returns the configured object type encoding mode.
  [[nodiscard]] constexpr object_type_encoding encoding() const noexcept {
    return encoding_;
  }

  /// Changes the configured object type encoding mode.
  constexpr inspector_config& encoding(object_type_encoding mode) noexcept {
    encoding_ = mode;
    return *this;
  }

private:
  caf::actor_handle_codec* codec_ = nullptr;
  const type_id_mapper* mapper_ = nullptr;
  object_type_encoding encoding_ = object_type_encoding::default_mode;
};

} // namespace caf
