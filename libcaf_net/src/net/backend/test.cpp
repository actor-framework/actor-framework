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

#include "caf/net/backend/test.hpp"

#include "caf/expected.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/basp/application.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/raise_error.hpp"
#include "caf/send.hpp"

namespace caf {
namespace net {
namespace backend {

test::test(middleman& mm)
  : middleman_backend("test"), mm_(mm), proxies_(mm.system(), *this) {
  // nop
}

test::~test() {
  // nop
}

error test::init() {
  return none;
}

endpoint_manager_ptr test::peer(const node_id& id) {
  return get_peer(id).second;
}

void test::resolve(const uri& locator, const actor& listener) {
  auto id = locator.authority_only();
  if (id)
    peer(make_node_id(*id))->resolve(locator, listener);
  else
    anon_send(listener, error(basp::ec::invalid_locator));
}

strong_actor_ptr test::make_proxy(node_id nid, actor_id aid) {
  using impl_type = actor_proxy_impl;
  using hdl_type = strong_actor_ptr;
  actor_config cfg;
  return make_actor<impl_type, hdl_type>(aid, nid, &mm_.system(), cfg,
                                         peer(nid));
}

void test::set_last_hop(node_id*) {
  // nop
}

test::peer_entry& test::emplace(const node_id& peer_id, stream_socket first,
                                stream_socket second) {
  using transport_type = stream_transport<basp::application>;
  nonblocking(second, true);
  auto mpx = mm_.mpx();
  auto mgr = make_endpoint_manager(mpx, mm_.system(),
                                   transport_type{second,
                                                  basp::application{proxies_}});
  if (auto err = mgr->init()) {
    CAF_LOG_ERROR("mgr->init() failed: " << mm_.system().render(err));
    CAF_RAISE_ERROR("mgr->init() failed");
  }
  mpx->register_reading(mgr);
  auto& result = peers_[peer_id];
  result = std::make_pair(first, std::move(mgr));
  return result;
}

test::peer_entry& test::get_peer(const node_id& id) {
  auto i = peers_.find(id);
  if (i != peers_.end())
    return i->second;
  auto sockets = make_stream_socket_pair();
  if (!sockets) {
    CAF_LOG_ERROR("make_stream_socket_pair failed: "
                  << mm_.system().render(sockets.error()));
    CAF_RAISE_ERROR("make_stream_socket_pair failed");
  }
  return emplace(id, sockets->first, sockets->second);
}

} // namespace backend
} // namespace net
} // namespace caf
