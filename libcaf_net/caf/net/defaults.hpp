// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <cstdint>

#include "caf/detail/net_export.hpp"

// -- hard-coded default values for various CAF options ------------------------

namespace caf::defaults::middleman {

/// Maximum number of cached buffers for sending payloads.
CAF_NET_EXPORT extern const size_t max_payload_buffers;

/// Maximum number of cached buffers for sending headers.
CAF_NET_EXPORT extern const size_t max_header_buffers;

/// Port to listen on for tcp.
CAF_NET_EXPORT extern const uint16_t tcp_port;

/// Caps how much Bytes a stream transport pushes to its write buffer before
/// stopping to read from its message queue. Default TCP send buffer is 16kB (at
/// least on Linux).
constexpr auto stream_output_buf_cap = size_t{32768};

} // namespace caf::defaults::middleman
