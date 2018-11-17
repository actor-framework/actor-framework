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

#pragma once

#include "caf/logger.hpp"
#include "caf/ref_counted.hpp"

#include "caf/io/fwd.hpp"

#include "caf/io/network/operation.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/acceptor_manager.hpp"

namespace caf {
namespace io {
namespace network {

/// An acceptor is responsible for accepting incoming connections.
class acceptor : public event_handler {
public:
  /// A manager providing the `accept` member function.
  using manager_type = acceptor_manager;

  /// A smart pointer to an acceptor manager.
  using manager_ptr = intrusive_ptr<manager_type>;

  acceptor(default_multiplexer& backend_ref, native_socket sockfd);

  /// Returns the accepted socket. This member function should
  /// be called only from the `new_connection` callback.
  inline native_socket& accepted_socket() {
    return sock_;
  }

  /// Starts this acceptor, forwarding all incoming connections to
  /// `manager`. The intrusive pointer will be released after the
  /// acceptor has been closed or an IO error occured.
  void start(acceptor_manager* mgr);

  /// Activates the acceptor.
  void activate(acceptor_manager* mgr);

  void removed_from_loop(operation op) override;

  void graceful_shutdown() override;

protected:
  template <class Policy>
  void handle_event_impl(io::network::operation op, Policy& policy) {
    CAF_LOG_TRACE(CAF_ARG(fd()) << CAF_ARG(op));
    if (mgr_ && op == operation::read) {
      native_socket sockfd = invalid_native_socket;
      if (policy.try_accept(sockfd, fd())) {
        if (sockfd != invalid_native_socket) {
          sock_ = sockfd;
          mgr_->new_connection();
        }
      }
    }
  }

private:
  manager_ptr mgr_;
  native_socket sock_;
};

} // namespace network
} // namespace io
} // namespace caf
