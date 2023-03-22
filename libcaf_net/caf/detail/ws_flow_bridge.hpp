// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/flow_bridge_base.hpp"
#include "caf/fwd.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/upper_layer.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::detail {

/// Convenience alias for referring to the base type of @ref flow_bridge.
template <class Trait, class Base>
using ws_flow_bridge_base_t
  = detail::flow_bridge_base<Base, net::web_socket::lower_layer, Trait>;

/// Translates between a message-oriented transport and data flows.
template <class Trait, class Base>
class ws_flow_bridge : public ws_flow_bridge_base_t<Trait, Base> {
public:
  using super = ws_flow_bridge_base_t<Trait, Base>;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using super::super;

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
