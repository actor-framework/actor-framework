// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/multiplexer_base.hpp"

#include "caf/detail/io_network_deprecated.hpp"

#include <memory>

namespace caf::io::network {

using multiplexer CAF_IO_NETWORK_DEPRECATED = multiplexer_base;

using multiplexer_ptr CAF_IO_NETWORK_DEPRECATED
  = std::unique_ptr<multiplexer_base>;

} // namespace caf::io::network
