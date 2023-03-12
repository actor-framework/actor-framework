// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/flow_connector.hpp"
#include "caf/net/web_socket/acceptor.hpp"

namespace caf::detail {

/// Calls an `OnRequest` handler with a @ref request object and passes the
/// generated buffers to the @ref flow_bridge.
template <class Trait, class... Ts>
class ws_flow_connector_request_impl : public flow_connector<Trait> {
public:
  using super = flow_connector<Trait>;

  using result_type = typename super::result_type;

  using acceptor_base_type = net::web_socket::acceptor<Ts...>;

  using acceptor_type = net::web_socket::acceptor_impl<Trait, Ts...>;

  using app_res_type = typename acceptor_type::app_res_type;

  using producer_type = async::blocking_producer<app_res_type>;

  using on_request_cb_type
    = shared_callback_ptr<void(const settings&, acceptor_base_type&)>;

  template <class T>
  ws_flow_connector_request_impl(on_request_cb_type on_request, T&& out)
    : on_request_(std::move(on_request)), out_(std::forward<T>(out)) {
    // nop
  }

  result_type on_request(const settings& cfg) override {
    acceptor_type acc;
    (*on_request_)(cfg, acc);
    if (acc.accepted()) {
      out_.push(acc.app_resources);
      auto& [pull, push] = acc.ws_resources;
      return {error{}, std::move(pull), std::move(push)};
    } else if (auto&& err = std::move(acc).reject_reason()) {
      return {std::move(err), {}, {}};
    } else {
      auto def_err = make_error(sec::runtime_error,
                                "WebSocket request rejected without reason");
      return {std::move(def_err), {}, {}};
    }
  }

private:
  on_request_cb_type on_request_;
  producer_type out_;
};

} // namespace caf::detail
