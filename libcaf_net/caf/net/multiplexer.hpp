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

#include "caf/detail/net_export.hpp"
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
class CAF_NET_EXPORT multiplexer {
public:
  // -- member types -----------------------------------------------------------

  using pollfd_list = std::vector<pollfd>;

  using manager_list = std::vector<socket_manager_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  /// @param parent Points to the owning middleman instance. May be `nullptr`
  ///               only for the purpose of unit testing if no @ref
  ///               socket_manager requires access to the @ref middleman or the
  ///               @ref actor_system.
  explicit multiplexer(middleman* parent);

  ~multiplexer();

  // -- initialization ---------------------------------------------------------

  error init();

  // -- properties -------------------------------------------------------------

  /// Returns the number of currently active socket managers.
  size_t num_socket_managers() const noexcept;

  /// Returns the index of `mgr` in the pollset or `-1`.
  ptrdiff_t index_of(const socket_manager_ptr& mgr);

  /// Returns the owning @ref middleman instance.
  middleman& owner();

  /// Returns the enclosing @ref actor_system.
  actor_system& system();

  // -- thread-safe signaling --------------------------------------------------

  /// Registers `mgr` for read events.
  /// @thread-safe
  void register_reading(const socket_manager_ptr& mgr);

  /// Registers `mgr` for write events.
  /// @thread-safe
  void register_writing(const socket_manager_ptr& mgr);

  /// Registers `mgr` for initialization in the multiplexer's thread.
  /// @thread-safe
  void init(const socket_manager_ptr& mgr);

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
  /// @thread-safe
  void shutdown();

protected:
  // -- utility functions ------------------------------------------------------

  /// Handles an I/O event on given manager.
  short handle(const socket_manager_ptr& mgr, short events, short revents);

  /// Adds a new socket manager to the pollset.
  void add(socket_manager_ptr mgr);

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

  /// Guards `write_handle_`.
  std::mutex write_lock_;

  /// Used for pushing updates to the multiplexer's thread.
  pipe_socket write_handle_;

  /// Points to the owning middleman.
  middleman* owner_;

  /// Signals whether shutdown has been requested.
  bool shutting_down_ = false;
};

} // namespace caf::net
