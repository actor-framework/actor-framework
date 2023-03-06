// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/flow_bridge_base.hpp"
#include "caf/detail/flow_connector.hpp"
#include "caf/fwd.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/net/web_socket/upper_layer.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::detail {

/// Convenience alias for referring to the base type of @ref flow_bridge.
template <class Trait>
using ws_flow_bridge_base_t
  = detail::flow_bridge_base<net::web_socket::upper_layer,
                             net::web_socket::lower_layer, Trait>;

/// Translates between a message-oriented transport and data flows.
template <class Trait>
class ws_flow_bridge : public ws_flow_bridge_base_t<Trait> {
public:
  using super = ws_flow_bridge_base_t<Trait>;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using connector_pointer = flow_connector_ptr<Trait>;

  using super::super;

  static std::unique_ptr<ws_flow_bridge> make(async::execution_context_ptr loop,
                                              connector_pointer conn) {
    return std::make_unique<ws_flow_bridge>(std::move(loop), std::move(conn));
  }

  bool write(const output_type& item) override {
    if (super::trait_.converts_to_binary(item)) {
      super::down_->begin_binary_message();
      auto& bytes = super::down_->binary_message_buffer();
      return super::trait_.convert(item, bytes)
             && super::down_->end_binary_message();
    } else {
      super::down_->begin_text_message();
      auto& text = super::down_->text_message_buffer();
      return super::trait_.convert(item, text)
             && super::down_->end_text_message();
    }
  }

  // -- implementation of web_socket::lower_layer ------------------------------

  ptrdiff_t consume_binary(byte_span buf) override {
    if (!super::out_)
      return -1;
    input_type val;
    if (!super::trait_.convert(buf, val))
      return -1;
    if (super::out_.push(std::move(val)) == 0)
      super::down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

  ptrdiff_t consume_text(std::string_view buf) override {
    if (!super::out_)
      return -1;
    input_type val;
    if (!super::trait_.convert(buf, val))
      return -1;
    if (super::out_.push(std::move(val)) == 0)
      super::down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }
};

} // namespace caf::detail
