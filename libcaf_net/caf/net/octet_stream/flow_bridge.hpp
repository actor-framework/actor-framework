// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/detail/flow_bridge_initializer.hpp"
#include "caf/detail/net_export.hpp"

#include <memory>

namespace caf::net::octet_stream {

CAF_NET_EXPORT
std::unique_ptr<upper_layer>
make_flow_bridge(uint32_t read_buffer_size, uint32_t write_buffer_size,
                 detail::flow_bridge_initializer_ptr init);

template <class Trait>
std::unique_ptr<upper_layer>
make_flow_bridge(uint32_t read_buffer_size, uint32_t write_buffer_size,
                 Trait trait,
                 async::consumer_resource<typename Trait::input_type> pull,
                 async::producer_resource<typename Trait::output_type> push) {
  auto init = detail::make_flow_bridge_initializer(std::move(trait),
                                                   std::move(pull),
                                                   std::move(push));
  return make_flow_bridge(read_buffer_size, write_buffer_size, std::move(init));
}

} // namespace caf::net::octet_stream
