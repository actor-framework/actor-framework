// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/error.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/sec.hpp"

#include <tuple>

namespace caf::detail {

template <class Trait>
class flow_connector;

template <class Trait>
using flow_connector_ptr = std::shared_ptr<flow_connector<Trait>>;

/// Connects a flow bridge to input and output buffers.
template <class Trait>
class flow_connector {
public:
  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using pull_t = async::consumer_resource<input_type>;

  using push_t = async::producer_resource<output_type>;

  using result_type = std::tuple<error, pull_t, push_t>;

  using connect_event_type = cow_tuple<async::consumer_resource<output_type>,
                                       async::producer_resource<input_type>>;

  using connect_event_buf = async::spsc_buffer_ptr<connect_event_type>;

  virtual ~flow_connector() {
    // nop
  }

  virtual result_type on_request(const settings& cfg) = 0;

  /// Returns a trivial implementation that simply returns @p pull and @p push
  /// from @p on_request.
  static flow_connector_ptr<Trait> make_trivial(pull_t pull, push_t push);

  /// Returns an implementation for a basic server that simply creates connected
  /// buffer pairs.
  static flow_connector_ptr<Trait> make_basic_server(connect_event_buf buf);
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

/// A flow connector for basic servers that have no custom handshake logic.
template <class Trait>
class flow_connector_basic_server_impl : public flow_connector<Trait> {
public:
  using super = flow_connector<Trait>;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using result_type = typename super::result_type;

  using connect_event_buf = typename super::connect_event_buf;

  using connect_event = typename super::connect_event_type;

  using producer_type = async::blocking_producer<connect_event>;

  explicit flow_connector_basic_server_impl(connect_event_buf buf)
    : prod_(std::move(buf)) {
    // nop
  }

  result_type on_request(const settings&) override {
    auto [app_pull, srv_push] = async::make_spsc_buffer_resource<input_type>();
    auto [srv_pull, app_push] = async::make_spsc_buffer_resource<output_type>();
    prod_.push(connect_event{std::move(srv_pull), std::move(srv_push)});
    return {{}, std::move(app_pull), std::move(app_push)};
  }

private:
  producer_type prod_;
};

template <class Trait>
flow_connector_ptr<Trait>
flow_connector<Trait>::make_trivial(pull_t pull, push_t push) {
  using impl_t = detail::flow_connector_trivial_impl<Trait>;
  return std::make_shared<impl_t>(std::move(pull), std::move(push));
}

template <class Trait>
flow_connector_ptr<Trait>
flow_connector<Trait>::make_basic_server(connect_event_buf buf) {
  using impl_t = detail::flow_connector_basic_server_impl<Trait>;
  return std::make_shared<impl_t>(std::move(buf));
}

} // namespace caf::detail
