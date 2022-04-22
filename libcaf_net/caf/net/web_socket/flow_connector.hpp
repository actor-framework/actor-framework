// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/error.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/sec.hpp"

namespace caf::net::web_socket {

template <class Trait>
class flow_connector {
public:
  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using app_res_pair_type = async::resource_pair<input_type, output_type>;

  using ws_res_pair_type = async::resource_pair<output_type, input_type>;

  using producer_type = async::blocking_producer<app_res_pair_type>;

  virtual ~flow_connector() {
    // nop
  }

  error on_request(const settings& cfg, request<Trait>& req) {
    do_on_request(cfg, req);
    if (req.accepted()) {
      out_.push(req.app_resources_);
      return none;
    } else if (auto& err = req.reject_reason()) {
      return err;
    } else {
      return make_error(sec::runtime_error,
                        "WebSocket request rejected without reason");
    }
  }

protected:
  template <class T>
  explicit flow_connector(T&& out) : out_(std::forward<T>(out)) {
    // nop
  }

private:
  virtual void do_on_request(const settings& cfg, request<Trait>& req) = 0;

  producer_type out_;
};

template <class Trait>
using flow_connector_ptr = std::shared_ptr<flow_connector<Trait>>;

template <class OnRequest, class Trait>
class flow_connector_impl : public flow_connector<Trait> {
public:
  using super = flow_connector<Trait>;

  using producer_type = typename super::producer_type;

  template <class T>
  flow_connector_impl(T&& out, OnRequest on_request)
    : super(std::forward<T>(out)), on_request_(std::move(on_request)) {
    // nop
  }

private:
  void do_on_request(const settings& cfg, request<Trait>& req) {
    on_request_(cfg, req);
  }

  OnRequest on_request_;
};

} // namespace caf::net::web_socket
