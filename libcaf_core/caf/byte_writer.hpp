// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/serializer.hpp"
#include "caf/span.hpp"

#include <cstdio>

namespace caf {

/// Serializes inspectable objects to a sequence of bytes.
class CAF_CORE_EXPORT byte_writer : public serializer {
public:
  using serializer::serializer;

  ~byte_writer() override;

  /// Retrieves the serialized bytes.
  virtual const_byte_span bytes() const = 0;

  /// Clears any buffered data and resets the writer to its initial state.
  virtual void reset() = 0;
};

} // namespace caf
