// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/deserializer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Deserializes inspectable objects from a sequence of bytes.
class CAF_CORE_EXPORT byte_reader : public deserializer {
public:
  using deserializer::deserializer;

  ~byte_reader() override;

  /// Resets the reader and loads a sequence of bytes to deserialize from.
  virtual bool load_bytes(const_byte_span bytes) = 0;

  /// Returns whether the reader expects type ID lists using names instead of
  /// integers.
  [[nodiscard]] virtual bool use_type_names() const noexcept = 0;

  /// Configures whether the reader expects type ID lists using names instead
  /// of integers.
  virtual void use_type_names(bool value) noexcept = 0;

  /// Returns the type ID mapper used by the reader.
  [[nodiscard]] virtual const type_id_mapper* mapper() const noexcept = 0;

  /// Changes the type ID mapper for the reader.
  virtual void mapper(const type_id_mapper* ptr) noexcept = 0;
};

} // namespace caf
