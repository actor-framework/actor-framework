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

#include "caf/net/backend/tcp.hpp"

#include <string>

#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/basp/application.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/doorman.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/send.hpp"

namespace caf::net::backend {

tcp::tcp(middleman& mm)
  : middleman_backend("tcp"), mm_(mm), proxies_(mm.system(), *this) {
  // nop
}

tcp::~tcp() {
  // nop
}

error tcp::init() {
  uint16_t conf_port = get_or<uint16_t>(
    mm_.system().config(), "middleman.tcp-port", defaults::middleman::tcp_port);
  ip_endpoint ep;
  auto local_address = std::string("[::]:") + std::to_string(conf_port);
  if (auto err = detail::parse(local_address, ep))
    return err;
  auto acceptor = make_tcp_accept_socket(ep, true);
  if (!acceptor)
    return acceptor.error();
  auto acc_guard = make_socket_guard(*acceptor);
  nonblocking(acc_guard.socket(), true);
  auto port = local_port(*acceptor);
  if (!port)
    return port.error();
  listening_port_ = *port;
  CAF_LOG_INFO("doorman spawned on " << CAF_ARG(*port));
  auto doorman_uri = make_uri("tcp://doorman");
  if (!doorman_uri)
    return doorman_uri.error();
  auto& mpx = mm_.mpx();
  auto mgr = make_endpoint_manager(
    mpx, mm_.system(),
    doorman{acc_guard.release(), tcp::basp_application_factory{proxies_}});
  if (auto err = mgr->init()) {
    CAF_LOG_ERROR("mgr->init() failed: " << err);
    return err;
  }
  return none;
}

void tcp::stop() {
  for (const auto& p : peers_)
    proxies_.erase(p.first);
  peers_.clear();
}

expected<endpoint_manager_ptr> tcp::connect(const uri& locator) {
  auto auth = locator.authority();
  auto host = auth.host;
  if (auto hostname = get_if<std::string>(&host)) {
    for (const auto& addr : ip::resolve(*hostname)) {
      ip_endpoint ep{addr, auth.port};
      auto sock = make_connected_tcp_stream_socket(ep);
      if (!sock)
        continue;
      else
        return emplace(make_node_id(*locator.authority_only()), *sock);
    }
  }
  return sec::cannot_connect_to_node;
}

endpoint_manager_ptr tcp::peer(const node_id& id) {
  return get_peer(id);
}

void tcp::resolve(const uri& locator, const actor& listener) {
  auto id = locator.authority_only();
  if (id) {
    auto p = peer(make_node_id(*id));
    if (p == nullptr) {
      CAF_LOG_INFO("connecting to " << CAF_ARG(locator));
      auto res = connect(locator);
      if (!res)
        anon_send(listener, error(sec::cannot_connect_to_node));
      else
        p = *res;
    }
    p->resolve(locator, listener);
  } else {
    anon_send(listener, error(basp::ec::invalid_locator));
  }
}

strong_actor_ptr tcp::make_proxy(node_id nid, actor_id aid) {
  using impl_type = actor_proxy_impl;
  using hdl_type = strong_actor_ptr;
  actor_config cfg;
  return make_actor<impl_type, hdl_type>(aid, nid, &mm_.system(), cfg,
                                         peer(nid));
}

void tcp::set_last_hop(node_id*) {
  // nop
}

endpoint_manager_ptr tcp::get_peer(const node_id& id) {
  auto i = peers_.find(id);
  if (i != peers_.end())
    return i->second;
  return nullptr;
}

tcp::basp_application_factory::basp_application_factory(proxy_registry& proxies)
  : proxies_(proxies) {
  // nop
}

} // namespace caf::net::backend
