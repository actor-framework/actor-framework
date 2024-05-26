// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/internal/lp_flow_bridge.hpp"

#include "caf/net/lp/lower_layer.hpp"
#include "caf/net/lp/upper_layer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/internal/flow_bridge_base.hpp"

#include <cstddef>

namespace caf::net::lp {

namespace {

/// Convenience alias for referring to the base type of @ref flow_bridge.
using flow_bridge_base
  = internal::flow_bridge_base<net::lp::upper_layer, net::lp::lower_layer,
                               net::lp::frame>;

/// Translates between a message-oriented transport and data flows.
class flow_bridge : public flow_bridge_base {
public:
  using super = flow_bridge_base;

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

class client_flow_bridge : public flow_bridge {
public:
  // We consume the output type of the application.
  using pull_t = async::consumer_resource<net::lp::frame>;

  // We produce the input type of the application.
  using push_t = async::producer_resource<net::lp::frame>;

  client_flow_bridge(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  void abort(const error& err) override {
    super::abort(err);
    if (push_)
      push_.abort(err);
  }

  error start(net::lp::lower_layer* down) override {
    super::down_ = down;
    super::self_ref_ = down->manager()->as_disposable();
    return super::init(&down->mpx(), std::move(pull_), std::move(push_));
  }

private:
  pull_t pull_;
  push_t push_;
};

class server_flow_bridge : public flow_bridge {
public:
  using super = flow_bridge;

  using accept_event_t = net::accept_event<net::lp::frame>;

  server_flow_bridge(internal::lp_prodcuer_ptr producer)
    : producer_(std::move(producer)) {
    // nop
  }

  error start(net::lp::lower_layer* down) override {
    using net::lp::frame;
    CAF_ASSERT(down != nullptr);
    super::down_ = down;
    super::self_ref_ = down->manager()->as_disposable();
    auto [app_pull, push] = async::make_spsc_buffer_resource<frame>();
    auto [pull, app_push] = async::make_spsc_buffer_resource<frame>();
    auto event = accept_event_t{std::move(app_pull), std::move(app_push)};
    if (!producer_->push(event)) {
      return make_error(sec::runtime_error,
                        "Length-prefixed connection dropped: client canceled");
    }
    return super::init(&down->mpx(), std::move(pull), std::move(push));
  }

private:
  internal::lp_prodcuer_ptr producer_;
};

} // namespace

} // namespace caf::net::lp

namespace caf::internal {

std::unique_ptr<net::lp::upper_layer>
make_lp_flow_bridge(async::consumer_resource<net::lp::frame> pull,
                    async::producer_resource<net::lp::frame> push) {
  using impl_t = net::lp::client_flow_bridge;
  return std::make_unique<impl_t>(std::move(pull), std::move(push));
}

std::unique_ptr<net::lp::upper_layer>
make_lp_flow_bridge(lp_prodcuer_ptr producer) {
  using impl_t = net::lp::server_flow_bridge;
  return std::make_unique<impl_t>(std::move(producer));
}

} // namespace caf::internal
