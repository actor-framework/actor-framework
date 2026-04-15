// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/serializer.hpp"

namespace caf {

/// Serializes inspectable objects to a sequence of bytes.
class CAF_CORE_EXPORT byte_writer : public serializer {
public:
  ~byte_writer() override;

  /// Retrieves the serialized bytes.
  /// @warning This span becomes invalid when calling any non-const member
  ///          function on the writer object.
  [[nodiscard]] virtual const_byte_span bytes() const = 0;

  /// Clears any buffered data and resets the writer to its initial state.
  virtual void reset() = 0;

  /// Jumps `num_bytes` forward by inserting zeros at the end of the buffer.
  /// @returns the offset where the zeros were inserted.
  [[nodiscard]] virtual size_t skip(size_t num_bytes) = 0;

  /// Overrides the buffer at `offset` with `content`.
  /// @returns `true` if the buffer had enough space to hold `content`, `false`
  ///          otherwise.
  [[nodiscard]] virtual bool
  update(size_t offset, const_byte_span content) noexcept
    = 0;
};

} // namespace caf
