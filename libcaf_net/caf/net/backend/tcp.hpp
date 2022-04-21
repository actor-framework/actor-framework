// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <map>
#include <mutex>

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
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
class CAF_NET_EXPORT tcp : public middleman_backend {
public:
  using peer_map = std::map<node_id, endpoint_manager_ptr>;

  using emplace_return_type = std::pair<peer_map::iterator, bool>;

  // -- constructors, destructors, and assignment operators --------------------

  tcp(middleman& mm);

  ~tcp() override;

  // -- interface functions ----------------------------------------------------

  error init() override;

  void stop() override;

  expected<endpoint_manager_ptr> get_or_connect(const uri& locator) override;

  endpoint_manager_ptr peer(const node_id& id) override;

  void resolve(const uri& locator, const actor& listener) override;

  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override;

  void set_last_hop(node_id*) override;

  // -- properties -------------------------------------------------------------

  uint16_t port() const noexcept override;

  template <class Handle>
  expected<endpoint_manager_ptr>
  emplace(const node_id& peer_id, Handle socket_handle) {
    using transport_type = stream_transport<basp::application>;
    if (auto err = nonblocking(socket_handle, true))
      return err;
    auto mpx = mm_.mpx();
    basp::application app{proxies_};
    auto mgr = make_endpoint_manager(
      mpx, mm_.system(), transport_type{socket_handle, std::move(app)});
    if (auto err = mgr->init()) {
      CAF_LOG_ERROR("mgr->init() failed: " << err);
      return err;
    }
    mpx->register_reading(mgr);
    emplace_return_type res;
    {
      const std::lock_guard<std::mutex> lock(lock_);
      res = peers_.emplace(peer_id, std::move(mgr));
    }
    if (res.second)
      return res.first->second;
    else
      return make_error(sec::runtime_error, "peer_id already exists");
  }

private:
  endpoint_manager_ptr get_peer(const node_id& id);

  middleman& mm_;

  peer_map peers_;

  proxy_registry proxies_;

  uint16_t listening_port_;

  std::mutex lock_;
};

} // namespace caf::net::backend
