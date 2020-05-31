/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <map>

#include "caf/detail/net_export.hpp"
#include "caf/net/basp/application.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/middleman_backend.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/node_id.hpp"

namespace caf::net::backend {

/// Minimal backend for tcp communication.
/// @warning this backend is *not* thread safe.
class CAF_NET_EXPORT tcp : public middleman_backend {
public:
  // -- constructors, destructors, and assignment operators --------------------

  tcp(middleman& mm);

  ~tcp() override;

  // -- interface functions ----------------------------------------------------

  error init() override;

  void stop() override;

  endpoint_manager_ptr connect(const uri& locator);

  endpoint_manager_ptr peer(const node_id& id) override;

  void publish(actor handle, const uri& locator) override;

  void resolve(const uri& locator, const actor& listener) override;

  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override;

  void set_last_hop(node_id*) override;

  void publish();

  // -- properties -------------------------------------------------------------

  tcp_stream_socket socket(const node_id& peer_id) {
    return socket_cast<tcp_stream_socket>(peers_[peer_id]->handle());
  }

  uint16_t port() const noexcept override {
    return listening_port_;
  }

private:
  class basp_application_factory {
  public:
    using application_type = basp::application;

    basp_application_factory(proxy_registry& proxies);
    ~basp_application_factory();

    template <class Parent>
    error init(Parent&) {
      return none;
    }

    application_type make() const {
      return application_type{proxies_};
    }

  private:
    proxy_registry& proxies_;
  };

  template <class Handle>
  endpoint_manager_ptr& emplace(const node_id& peer_id, Handle socket_handle) {
    using transport_type = stream_transport<basp::application>;
    nonblocking(socket_handle, true);
    auto mpx = mm_.mpx();
    basp::application app{proxies_};
    auto mgr = make_endpoint_manager(
      mpx, mm_.system(), transport_type{socket_handle, std::move(app)});
    if (auto err = mgr->init()) {
      CAF_LOG_ERROR("mgr->init() failed: " << err);
      CAF_RAISE_ERROR("mgr->init() failed");
    }
    mpx->register_reading(mgr);
    return peers_[peer_id];
  }

  endpoint_manager_ptr get_peer(const node_id& id);

  middleman& mm_;

  std::map<node_id, endpoint_manager_ptr> peers_;

  proxy_registry proxies_;

  uint16_t listening_port_;
};

} // namespace caf::net::backend
