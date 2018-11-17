/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/network/acceptor.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {
namespace network {

acceptor::acceptor(default_multiplexer& backend_ref, native_socket sockfd)
    : event_handler(backend_ref, sockfd),
      sock_(invalid_native_socket) {
  // nop
}

void acceptor::start(acceptor_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_));
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void acceptor::activate(acceptor_manager* mgr) {
  if (!mgr_) {
    mgr_.reset(mgr);
    event_handler::activate();
  }
}

void acceptor::removed_from_loop(operation op) {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_) << CAF_ARG(op));
  if (op == operation::read)
    mgr_.reset();
}

void acceptor::graceful_shutdown() {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_));
  // Ignore repeated calls.
  if (state_.shutting_down)
    return;
  state_.shutting_down = true;
  // Shutdown socket activity.
  shutdown_both(fd_);
}

} // namespace network
} // namespace io
} // namespace caf
