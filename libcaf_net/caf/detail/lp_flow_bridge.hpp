// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/lp/lower_layer.hpp"
#include "caf/net/lp/upper_layer.hpp"

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/flow_bridge_base.hpp"
#include "caf/fwd.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::detail {

/// Convenience alias for referring to the base type of @ref flow_bridge.
using lp_flow_bridge_base
  = flow_bridge_base<net::lp::upper_layer, net::lp::lower_layer,
                     net::lp::frame>;

/// Translates between a message-oriented transport and data flows.
class lp_flow_bridge : public lp_flow_bridge_base {
public:
  using super = lp_flow_bridge_base;

  using super::super;

  bool write(const net::lp::frame& item) override {
    super::down_->begin_message();
    auto& bytes = super::down_->message_buffer();
    auto src = item.bytes();
    bytes.insert(bytes.end(), src.begin(), src.end());
    return super::down_->end_message();
  }

  // -- implementation of lp::lower_layer --------------------------------------

  ptrdiff_t consume(byte_span buf) override {
    if (!super::out_)
      return -1;
    if (super::out_.push(net::lp::frame{buf}) == 0)
      super::down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }
};

} // namespace caf::detail
