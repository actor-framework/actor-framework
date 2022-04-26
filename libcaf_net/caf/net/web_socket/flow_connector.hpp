// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/error.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/sec.hpp"

#include <tuple>

namespace caf::net::web_socket {

/// Connects a @ref flow_bridge to input and output buffers.
template <class Trait>
class flow_connector {
public:
  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using result_type = std::tuple<error, async::consumer_resource<input_type>,
                                 async::producer_resource<output_type>>;

  virtual ~flow_connector() {
    // nop
  }

  virtual result_type on_request(const settings& cfg) = 0;
};

template <class Trait>
using flow_connector_ptr = std::shared_ptr<flow_connector<Trait>>;

/// Calls an `OnRequest` handler with a @ref request object and passes the
/// generated buffers to the @ref flow_bridge.
template <class OnRequest, class Trait, class... Ts>
class flow_connector_request_impl : public flow_connector<Trait> {
public:
  using super = flow_connector<Trait>;

  using result_type = typename super::result_type;

  using request_type = request<Trait, Ts...>;

  using app_res_type = typename request_type::app_res_type;

  using producer_type = async::blocking_producer<app_res_type>;

  template <class T>
  flow_connector_request_impl(OnRequest&& on_request, T&& out)
    : on_request_(std::move(on_request)), out_(std::forward<T>(out)) {
    // nop
  }

  result_type on_request(const settings& cfg) override {
    request_type req;
    on_request_(cfg, req);
    if (req.accepted()) {
      out_.push(req.app_resources_);
      auto& [pull, push] = req.ws_resources_;
      return {error{}, std::move(pull), std::move(push)};
    } else if (auto& err = req.reject_reason()) {
      return {err, {}, {}};
    } else {
      auto def_err = make_error(sec::runtime_error,
                                "WebSocket request rejected without reason");
      return {std::move(def_err), {}, {}};
    }
  }

private:
  OnRequest on_request_;
  producer_type out_;
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
    : pull_(std::move(pull)), push_(std::move(push)) { // nop
  }

  result_type on_request(const settings&) override {
    return {{}, std::move(pull_), std::move(push_)};
  }

private:
  pull_t pull_;
  push_t push_;
};

} // namespace caf::net::web_socket
