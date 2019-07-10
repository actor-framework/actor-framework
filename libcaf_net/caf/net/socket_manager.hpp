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

#include "caf/error.hpp"
#include "caf/net/socket.hpp"

namespace caf {
namespace net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class socket_manager {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit socket_manager(socket handle);

  virtual ~socket_manager();

  socket_manager(const socket_manager&) = delete;

  socket_manager& operator=(const socket_manager&) = delete;

  // -- properties -------------------------------------------------------------

  socket handle() const noexcept {
    return handle_;
  }

  // -- pure virtual member functions ------------------------------------------

  /// Called whenever the socket received new data.
  virtual error handle_read_event() = 0;

  /// Called whenever the socket is allowed to send additional data.
  virtual error handle_write_event() = 0;

  /// Called if the remote side becomes unreachable.
  /// @param reason A default-constructed error object (no error) if the remote
  ///               host performed an ordinary shutdown (only for connection
  ///               oriented sockets), otherwise an error code as reported by
  ///               the operating system.
  virtual void handle_shutdown_event(error reason) = 0;

private:
  // -- member variables -------------------------------------------------------

  socket handle_;
};

} // namespace net
} // namespace caf
