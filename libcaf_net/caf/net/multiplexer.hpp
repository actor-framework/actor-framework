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

#include <memory>
#include <mutex>
#include <thread>

#include "caf/net/fwd.hpp"
#include "caf/net/operation.hpp"
#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/ref_counted.hpp"

extern "C" {

struct pollfd;

} // extern "C"

namespace caf::net {

/// Multiplexes any number of ::socket_manager objects with a ::socket.
class multiplexer : public std::enable_shared_from_this<multiplexer> {
public:
  // -- member types -----------------------------------------------------------

  using pollfd_list = std::vector<pollfd>;

  using manager_list = std::vector<socket_manager_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  multiplexer();

  ~multiplexer();

  error init();

  // -- properties -------------------------------------------------------------

  /// Returns the number of currently active socket managers.
  size_t num_socket_managers() const noexcept;

  /// Returns the index of `mgr` in the pollset or `-1`.
  ptrdiff_t index_of(const socket_manager_ptr& mgr);

  // -- thread-safe signaling --------------------------------------------------

  /// Registers `mgr` for read events.
  /// @thread-safe
  void register_reading(const socket_manager_ptr& mgr);

  /// Registers `mgr` for write events.
  /// @thread-safe
  void register_writing(const socket_manager_ptr& mgr);

  /// Unregisters `mgr` for read and write events.
  /// @thread-safe
  void unregister_manager(const socket_manager_ptr& mgr);

  /// Closes the pipe for signaling updates to the multiplexer. After closing
  /// the pipe, calls to `update` no longer have any effect.
  /// @thread-safe
  void close_pipe();

  // -- control flow -----------------------------------------------------------

  /// Polls I/O activity once and runs all socket event handlers that become
  /// ready as a result.
  bool poll_once(bool blocking);

  /// Sets the thread ID to `std::this_thread::id()`.
  void set_thread_id();

  /// Polls until no socket event handler remains.
  void run();

  /// Signals the multiplexer to initiate shutdown.
  /// @returns true if manual triggering is needed, false if not.
  /// @thread-safe
  bool shutdown();

protected:
  // -- utility functions ------------------------------------------------------

  /// Handles an I/O event on given manager.
  short handle(const socket_manager_ptr& mgr, short events, short revents);

  /// Adds a new socket manager to the pollset.
  void add(socket_manager_ptr mgr);

  /// Deletes a known socket manager from the pollset.
  void del(const socket_manager_ptr& mgr);

  /// Deletes a known socket manager from the pollset.
  void del(ptrdiff_t index);

  /// Writes `opcode` and pointer to `mgr` the the pipe for handling an event
  /// later via the pollset updater.
  void write_to_pipe(uint8_t opcode, const socket_manager_ptr& mgr);

  // -- member variables -------------------------------------------------------

  /// Bookkeeping data for managed sockets.
  pollfd_list pollset_;

  /// Maps sockets to their owning managers by storing the managers in the same
  /// order as their sockets appear in `pollset_`.
  manager_list managers_;

  /// Stores the ID of the thread this multiplexer is running in. Set when
  /// calling `init()`.
  std::thread::id tid_;

  /// Used for pushing updates to the multiplexer's thread.
  pipe_socket write_handle_;

  /// Guards `write_handle_`.
  std::mutex write_lock_;

  /// Signals shutdown has been requested.
  bool will_shutdown_;
};

/// @relates multiplexer
using multiplexer_ptr = std::shared_ptr<multiplexer>;

/// @relates multiplexer
using weak_multiplexer_ptr = std::weak_ptr<multiplexer>;

} // namespace caf::net
