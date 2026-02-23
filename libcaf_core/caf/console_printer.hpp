// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/term.hpp"

#include <cstddef>
#include <memory>

namespace caf {

/// Interface for redirecting text output from `println` to a custom sink.
class CAF_CORE_EXPORT console_printer {
public:
  virtual ~console_printer();

  /// Prints the given text to the output stream.
  virtual void print(term color, const char* buf, size_t len) = 0;
};

} // namespace caf
