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

#include "caf/logger.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/send.hpp"

namespace caf::net {

/// A doorman accepts TCP connections and creates stream_transports to handle
/// them.
template <class Factory>
class doorman {
public:
  // -- member types -----------------------------------------------------------

  using factory_type = Factory;

  using application_type = typename Factory::application_type;

  // -- constructors, destructors, and assignment operators --------------------

  explicit doorman(net::tcp_accept_socket acceptor, factory_type factory)
    : acceptor_(acceptor), factory_(std::move(factory)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  net::tcp_accept_socket handle() {
    return acceptor_;
  }

  // -- member functions -------------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    // TODO: is initializing application factory nessecary?
    if (auto err = factory_.init(parent))
      return err;
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    auto x = net::accept(acceptor_);
    if (!x) {
      CAF_LOG_ERROR("accept failed:" << x.error());
      return false;
    }
    auto mpx = parent.multiplexer();
    if (!mpx) {
      CAF_LOG_DEBUG("unable to get multiplexer from parent");
      return false;
    }
    auto child = make_endpoint_manager(
      mpx, parent.system(),
      stream_transport<application_type>{*x, factory_.make()});
    if (auto err = child->init())
      return false;
    return true;
  }

  template <class Parent>
  bool handle_write_event(Parent&) {
    CAF_LOG_ERROR("doorman received write event");
    return false;
  }

  template <class Parent>
  void
  resolve(Parent&, [[maybe_unused]] const uri& locator, const actor& listener) {
    CAF_LOG_ERROR("doorman called to resolve" << CAF_ARG(locator));
    anon_send(listener, resolve_atom_v, "doormen cannot resolve paths");
  }

  void new_proxy(endpoint_manager&, const node_id& peer, actor_id id) {
    CAF_LOG_ERROR("doorman received new_proxy" << CAF_ARG(peer) << CAF_ARG(id));
    CAF_IGNORE_UNUSED(peer);
    CAF_IGNORE_UNUSED(id);
  }

  void local_actor_down(endpoint_manager&, const node_id& peer, actor_id id,
                        error reason) {
    CAF_LOG_ERROR("doorman received local_actor_down"
                  << CAF_ARG(peer) << CAF_ARG(id) << CAF_ARG(reason));
    CAF_IGNORE_UNUSED(peer);
    CAF_IGNORE_UNUSED(id);
    CAF_IGNORE_UNUSED(reason);
  }

  template <class Parent>
  void timeout(Parent&, [[maybe_unused]] const std::string& tag,
               [[maybe_unused]] uint64_t id) {
    CAF_LOG_ERROR("doorman received timeout" << CAF_ARG(tag) << CAF_ARG(id));
  }

  void handle_error([[maybe_unused]] sec err) {
    CAF_LOG_ERROR("doorman encounterd error: " << err);
  }

private:
  net::tcp_accept_socket acceptor_;

  factory_type factory_;
};

} // namespace caf::net
