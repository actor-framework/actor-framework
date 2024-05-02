// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/accept_event.hpp"
#include "caf/net/lp/upper_layer.hpp"

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/fwd.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf::detail {

CAF_NET_EXPORT
std::unique_ptr<net::lp::upper_layer>
make_lp_flow_bridge(async::consumer_resource<net::lp::frame> pull,
                    async::producer_resource<net::lp::frame> push);

using lp_prodcuer_ptr = std::shared_ptr<
  async::blocking_producer<net::accept_event<net::lp::frame>>>;

CAF_NET_EXPORT
std::unique_ptr<net::lp::upper_layer>
make_lp_flow_bridge(lp_prodcuer_ptr producer);

} // namespace caf::detail
