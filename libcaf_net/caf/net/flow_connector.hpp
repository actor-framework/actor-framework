// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/error.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/sec.hpp"

#include <tuple>

namespace caf::net {

/// Connects a flow bridge to input and output buffers.
template <class Trait>
class flow_connector {
public:
  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using pull_t = async::consumer_resource<input_type>;

  using push_t = async::producer_resource<output_type>;

  using result_type = std::tuple<error, pull_t, push_t>;

  virtual ~flow_connector() {
    // nop
  }

  virtual result_type on_request(const settings& cfg) = 0;

  /// Returns a trivial implementation that simply returns @p pull and @p push
  /// from @p on_request.
  static flow_connector_ptr<Trait> make_trivial(pull_t pull, push_t push);
};

/// Trivial flow connector that passes its constructor arguments to the
/// @ref flow_bridge.
template <class Trait>
class flow_connector_trivial_impl : public flow_connector<Trait> {
public:
  using super = flow_connector<Trait>;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using pull_t = async::consumer_resource<input_type>;

  using push_t = async::producer_resource<output_type>;

  using result_type = typename super::result_type;

  flow_connector_trivial_impl(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  result_type on_request(const settings&) override {
    return {{}, std::move(pull_), std::move(push_)};
  }

private:
  pull_t pull_;
  push_t push_;
};

template <class Trait>
flow_connector_ptr<Trait>
flow_connector<Trait>::make_trivial(pull_t pull, push_t push) {
  using impl_t = flow_connector_trivial_impl<Trait>;
  return std::make_shared<impl_t>(std::move(pull), std::move(push));
}

} // namespace caf::net
