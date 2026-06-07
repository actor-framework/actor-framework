// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/stream_socket.hpp"

#include "caf/detail/net_export.hpp"
#include "caf/expected.hpp"
#include "caf/timespan.hpp"

#include <cstdint>
#include <string>

namespace caf::detail {

/// Abstract interface for establishing outbound TCP connections. Inject a
/// custom implementation via `with_t::client::connector()` to intercept the
/// connection step (e.g. in unit tests using connected socket pairs).
class CAF_NET_EXPORT connector {
public:
  virtual ~connector();

  virtual expected<net::stream_socket>
  connect(const std::string& host, uint16_t port, timespan connection_timeout,
          size_t max_retry_count, timespan retry_delay)
    = 0;
};

} // namespace caf::detail
