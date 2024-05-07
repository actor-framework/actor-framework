// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/internal/ws_flow_bridge.hpp"

#include "caf/net/socket_manager.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/internal/flow_bridge_base.hpp"

namespace caf::net::web_socket {

namespace {

/// Convenience alias for referring to the base type of @ref flow_bridge.
template <class Base>
using flow_bridge_base_t
  = internal::flow_bridge_base<Base, net::web_socket::lower_layer,
                               net::web_socket::frame>;

/// Translates between a message-oriented transport and data flows.
template <class Base>
class flow_bridge : public flow_bridge_base_t<Base> {
public:
  using super = flow_bridge_base_t<Base>;

  using super::super;

  bool write(const frame& item) override {
    if (item.is_binary()) {
      super::down_->begin_binary_message();
      auto& bytes = super::down_->binary_message_buffer();
      auto src = item.as_binary();
      bytes.insert(bytes.end(), src.begin(), src.end());
      return super::down_->end_binary_message();
    } else {
      super::down_->begin_text_message();
      auto& text = super::down_->text_message_buffer();
      auto src = item.as_text();
      text.insert(text.end(), src.begin(), src.end());
      return super::down_->end_text_message();
    }
  }

  // -- implementation of web_socket::lower_layer ------------------------------

  ptrdiff_t consume_binary(byte_span buf) override {
    if (!super::out_)
      return -1;
    if (super::out_.push(frame{buf}) == 0)
      super::down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

  ptrdiff_t consume_text(std::string_view buf) override {
    if (!super::out_)
      return -1;
    if (super::out_.push(frame{buf}) == 0)
      super::down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }
};

class flow_bridge_impl : public flow_bridge<upper_layer> {
public:
  using super = flow_bridge<upper_layer>;

  using pull_t = async::consumer_resource<frame>;

  using push_t = async::producer_resource<frame>;

  flow_bridge_impl(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  error start(lower_layer* down_ptr) override {
    CAF_ASSERT(down_ptr != nullptr);
    super::down_ = down_ptr;
    return super::init(&down_ptr->mpx(), std::move(pull_), std::move(push_));
  }

private:
  pull_t pull_;
  push_t push_;
};

/// Specializes the WebSocket flow bridge for the server side.
class flow_bridge_acceptor : public flow_bridge<upper_layer::server> {
public:
  using super = flow_bridge<upper_layer::server>;

  explicit flow_bridge_acceptor(detail::ws_conn_acceptor_ptr wca)
    : wca_(std::move(wca)) {
    // nop
  }

  error start(lower_layer* down_ptr) override {
    if (wcs_ == nullptr) {
      return make_error(sec::runtime_error,
                        "WebSocket: called start without prior accept");
    }
    this->self_ref_ = down_ptr->manager()->as_disposable();
    super::down_ = down_ptr;
    auto res = wcs_->start();
    wcs_ = nullptr;
    if (!res) {
      return std::move(res.error());
    }
    auto [pull, push] = *res;
    return super::init(&down_ptr->mpx(), std::move(pull), std::move(push));
  }

  error accept(const net::http::request_header& hdr) override {
    auto wcs = wca_->accept(hdr);
    if (!wcs) {
      return std::move(wcs.error());
    }
    wcs_ = *wcs;
    return {};
  }

private:
  detail::ws_conn_acceptor_ptr wca_;
  detail::ws_conn_starter_ptr wcs_;
};

} // namespace

} // namespace caf::net::web_socket

namespace caf::internal {

std::unique_ptr<net::web_socket::upper_layer>
make_ws_flow_bridge(async::consumer_resource<net::web_socket::frame> pull,
                    async::producer_resource<net::web_socket::frame> push) {
  using impl_t = net::web_socket::flow_bridge_impl;
  return std::make_unique<impl_t>(std::move(pull), std::move(push));
}

std::unique_ptr<net::web_socket::upper_layer::server>
make_ws_flow_bridge(detail::ws_conn_acceptor_ptr wca) {
  using impl_t = net::web_socket::flow_bridge_acceptor;
  return std::make_unique<impl_t>(std::move(wca));
}

} // namespace caf::internal
