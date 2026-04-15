// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/byte_reader.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Deserializes inspectable objects from textual formats.
class CAF_CORE_EXPORT text_reader : public byte_reader {
public:
  using byte_reader::byte_reader;

  ~text_reader() override;

  /// Returns the suffix for generating type annotation fields for variant
  /// fields. For example, CAF inserts field called "@foo${field_type_suffix}"
  /// for a variant field called "foo".
  [[nodiscard]] virtual std::string_view field_type_suffix() const noexcept = 0;

  /// Configures the suffix for generating type annotation fields.
  virtual void field_type_suffix(std::string_view suffix) noexcept = 0;

  /// Returns the type ID mapper used by the reader.
  [[nodiscard]] virtual const type_id_mapper* mapper() const noexcept = 0;

  /// Changes the type ID mapper for the reader.
  virtual void mapper(const type_id_mapper* ptr) noexcept = 0;
};

} // namespace caf
