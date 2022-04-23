// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <cstdint>

// -- hard-coded default values for various CAF options ------------------------

namespace caf::defaults::middleman {

/// Maximum number of cached buffers for sending payloads.
constexpr size_t max_payload_buffers = 100;

/// Maximum number of cached buffers for sending headers.
constexpr size_t max_header_buffers = 10;

/// Port to listen on for tcp.
constexpr uint16_t tcp_port = 0;

} // namespace caf::defaults::middleman
