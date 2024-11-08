// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/deserializer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/span.hpp"

#include <cstdio>

namespace caf {

/// Deserializes inspectable objects from a sequence of bytes.
class CAF_CORE_EXPORT byte_reader : public deserializer {
public:
  using deserializer::deserializer;

  ~byte_reader() override;

  /// Resets the reader and loads a sequence of bytes to deserialize from.
  virtual bool load_bytes(span<const std::byte> bytes) = 0;
};

} // namespace caf
