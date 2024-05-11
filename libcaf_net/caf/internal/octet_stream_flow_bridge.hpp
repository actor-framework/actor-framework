// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/async/fwd.hpp"
#include "caf/detail/net_export.hpp"

#include <cstddef>
#include <memory>

namespace caf::internal {

std::unique_ptr<net::octet_stream::upper_layer>
make_octet_stream_flow_bridge(uint32_t read_buffer_size,
                              uint32_t write_buffer_size,
                              async::consumer_resource<std::byte> pull,
                              async::producer_resource<std::byte> push);

} // namespace caf::internal
