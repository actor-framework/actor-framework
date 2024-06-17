// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/web_socket/acceptor.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/expected.hpp"
#include "caf/intrusive_ptr.hpp"

#include <type_traits>

namespace caf::detail {

class CAF_NET_EXPORT ws_conn_starter : public ref_counted {
public:
  using resources = async::resource_pair<net::web_socket::frame>;

  ~ws_conn_starter() override;

  virtual expected<resources> start() = 0;
};

using ws_conn_starter_ptr = intrusive_ptr<ws_conn_starter>;

class CAF_NET_EXPORT ws_conn_acceptor : public ref_counted {
public:
  virtual ~ws_conn_acceptor();

  virtual expected<ws_conn_starter_ptr>
  accept(const net::http::request_header& hdr, net::socket_manager* mgr) = 0;

  virtual bool canceled() const noexcept = 0;

  virtual void abort(const error& reason) = 0;
};

using ws_conn_acceptor_ptr = intrusive_ptr<ws_conn_acceptor>;

template <class... Ts>
class ws_conn_starter_impl : public ws_conn_starter {
public:
  using super = ws_conn_starter;

  using accept_event
    = cow_tuple<async::consumer_resource<net::web_socket::frame>,
                async::producer_resource<net::web_socket::frame>, Ts...>;

  using producer_type = async::blocking_producer<accept_event>;

  // Note: this is shared with the connection factory. In general, this is
  //       *unsafe*. However, we exploit the fact that there is currently only
  //       one thread running in the multiplexer (which makes this safe).
  using shared_producer_type = std::shared_ptr<producer_type>;

  /// The pair of resources for the WebSocket worker.
  using resources = super::resources;

  ws_conn_starter_impl(shared_producer_type producer, accept_event event,
                       resources res)
    : producer_(std::move(producer)),
      event_(std::move(event)),
      res_(std::move(res)) {
    // nop
  }

  expected<resources> start() override {
    if (!producer_->push(event_)) {
      return make_error(sec::runtime_error,
                        "WebSocket connection dropped: client canceled");
    }
    return res_;
  }

private:
  shared_producer_type producer_;
  accept_event event_;
  resources res_;
};

template <class OnRequest, class... Ts>
class ws_conn_acceptor_impl : public ws_conn_acceptor {
public:
  using accept_event
    = cow_tuple<async::consumer_resource<net::web_socket::frame>,
                async::producer_resource<net::web_socket::frame>, Ts...>;

  using producer_type = async::blocking_producer<accept_event>;

  using shared_producer_type = std::shared_ptr<producer_type>;

  ws_conn_acceptor_impl(OnRequest on_request,
                        async::producer_resource<accept_event> push)
    : on_request_(std::move(on_request)) {
    producer_ = std::make_shared<producer_type>(push.try_open());
  }

  expected<ws_conn_starter_ptr> accept(const net::http::request_header& hdr,
                                       net::socket_manager* mgr) override {
    if (!producer_) {
      return make_error(sec::runtime_error,
                        "WebSocket connection dropped: client canceled");
    }
    ws_acceptor_impl<Ts...> acc{hdr, mgr};
    on_request_(acc);
    if (acc.accepted()) {
      using starter_t = ws_conn_starter_impl<Ts...>;
      auto res = make_counted<starter_t>(producer_, std::move(acc.app_event),
                                         std::move(acc.ws_resources));
      return res;
    }
    return std::move(acc) //
      .reject_reason()
      .or_else(sec::runtime_error, "WebSocket request rejected without reason");
  }

  bool canceled() const noexcept override {
    return !producer_ || producer_->canceled();
  }

  void abort(const error& reason) override {
    if (producer_) {
      producer_->abort(reason);
      producer_ = nullptr;
    }
  }

private:
  OnRequest on_request_;
  shared_producer_type producer_;
};

template <class OnRequest, class Acceptor>
struct ws_conn_acceptor_oracle;

template <class OnRequest, class... Ts>
struct ws_conn_acceptor_oracle<OnRequest, net::web_socket::acceptor<Ts...>> {
  using type = ws_conn_acceptor_impl<OnRequest, Ts...>;
};

template <class OnRequest, class Acceptor>
using ws_conn_acceptor_t =
  typename ws_conn_acceptor_oracle<OnRequest, Acceptor>::type;

} // namespace caf::detail
