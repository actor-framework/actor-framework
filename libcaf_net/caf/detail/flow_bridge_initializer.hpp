// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observer.hpp"

namespace caf::detail {

/// Initializes the inputs and outputs of a flow bridge.
class CAF_NET_EXPORT flow_bridge_initializer {
public:
  virtual ~flow_bridge_initializer();

  /// Connects the output of the bridge to the socket.
  virtual void
  init_outputs(flow::coordinator* self, flow::observer<std::byte> out)
    = 0;

  /// Connects the input of the socket to the bridge.
  virtual void
  init_inputs(flow::coordinator* self, flow::observable<std::byte> in)
    = 0;
};

template <class Trait>
class flow_bridge_initializer_impl : public flow_bridge_initializer {
public:
  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using pull_resource = async::consumer_resource<input_type>;

  using push_resource = async::producer_resource<output_type>;

  flow_bridge_initializer_impl(Trait trait, pull_resource pull,
                               push_resource push)
    : trait_(std::move(trait)), pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  void init_outputs(flow::coordinator* self,
                    flow::observer<std::byte> out) override {
    trait_.map_outputs(self, pull_.observe_on(self).as_observable())
      .subscribe(std::move(out));
  }

  void init_inputs(flow::coordinator* self,
                   flow::observable<std::byte> in) override {
    trait_.map_inputs(self, std::move(in)).subscribe(push_);
  }

private:
  Trait trait_;

  pull_resource pull_;

  push_resource push_;
};

template <class Trait>
auto make_flow_bridge_initializer(
  Trait trait, async::consumer_resource<typename Trait::input_type> pull,
  async::producer_resource<typename Trait::output_type> push) {
  using initializer = flow_bridge_initializer_impl<Trait>;
  return std::make_unique<initializer>(std::move(trait), std::move(pull),
                                       std::move(push));
}

/// A smart pointer to a `flow_bridge_initializer`.
using flow_bridge_initializer_ptr = std::unique_ptr<flow_bridge_initializer>;

} // namespace caf::detail
