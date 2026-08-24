// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/io_export.hpp"

#include <memory>

namespace caf::io::network {

/// Makes sure the multiplexer does not exit its event loop until the destructor
/// of this class has been called.
class CAF_IO_EXPORT multiplexer_supervisor {
public:
  virtual ~multiplexer_supervisor() noexcept;
};

/// Smart pointer for `multiplexer_supervisor`.
using multiplexer_supervisor_ptr = std::unique_ptr<multiplexer_supervisor>;

} // namespace caf::io::network
