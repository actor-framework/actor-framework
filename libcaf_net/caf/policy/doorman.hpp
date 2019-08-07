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

#include <vector>

#include "caf/detail/socket_sys_includes.hpp"
#include "caf/logger.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/tcp.hpp"
#include "caf/policy/scribe.hpp"
#include "caf/send.hpp"

namespace caf {
namespace policy {

/// A doorman accepts TCP connections and creates scribes to handle them.
class doorman {
public:
  doorman(net::stream_socket acceptor) : acceptor_(acceptor) {
    // nop
  }

  net::socket acceptor_;

  net::socket handle() {
    return acceptor_;
  }

  template <class Parent>
  error init(Parent& parent) {
    parent.application().init(parent);
    parent.mask_add(net::operation::read);
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    auto sck = net::tcp::accept(
      net::socket_cast<net::stream_socket>(acceptor_));
    if (!sck) {
      CAF_LOG_ERROR("accept failed:" << sck.error());
      return false;
    }
    auto mpx = parent.multiplexer();
    if (!mpx) {
      CAF_LOG_DEBUG(
        "could not acquire multiplexer to create a new endpoint manager");
      return false;
    }
    auto child = make_endpoint_manager(mpx, parent.system(), scribe{*sck},
                                       parent.application().make());
    if (auto err = child->init()) {
      return false;
    }
    return true;
  }

  template <class Parent>
  bool handle_write_event(Parent&) {
    CAF_LOG_ERROR("doorman received write event");
    return false;
  }

  template <class Parent>
  void resolve(Parent&, const std::string& path, actor listener) {
    CAF_LOG_ERROR("doorman called to resolve" << CAF_ARG(path));
    anon_send(listener, resolve_atom::value, "doorman cannot resolve paths");
  }

  template <class Parent>
  void timeout(Parent&, atom_value x, uint64_t id) {
    CAF_LOG_ERROR("doorman received timeout" << CAF_ARG(x) << CAF_ARG(id));
    CAF_IGNORE_UNUSED(x);
    CAF_IGNORE_UNUSED(id);
  }

  template <class Application>
  void handle_error(Application&, sec) {
    close(acceptor_);
  }
};

} // namespace policy
} // namespace caf
