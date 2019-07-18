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

namespace caf {
namespace net {

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

  /// @thread-safe
  void update(const socket_manager_ptr& mgr);

  // -- control flow -----------------------------------------------------------

  /// Polls I/O activity once and runs all socket event handlers that become
  /// ready as a result.
  bool poll_once(bool blocking);

  /// Polls until no socket event handler remains.
  void run();

  void handle_updates();

  void handle(const socket_manager_ptr& mgr, int mask);

protected:
  // -- convenience functions --------------------------------------------------

  void add(socket_manager_ptr mgr);

  // -- member variables -------------------------------------------------------

  /// Bookkeeping data for managed sockets.
  std::vector<pollfd> pollset_;

  /// Maps sockets to their owning managers by storing the managers in the same
  /// order as their sockets appear in `pollset_`.
  manager_list managers_;

  /// Managers that updated their event mask and need updating.
  manager_list dirty_managers_;

  /// Stores the ID of the thread this multiplexer is running in. Must be set
  /// by the subclass.
  std::thread::id tid_;

  ///
  pipe_socket write_handle_;

  ///
  std::mutex write_lock_;
};

/// @relates multiplexer
using multiplexer_ptr = std::shared_ptr<multiplexer>;

/// @relates multiplexer
using weak_multiplexer_ptr = std::weak_ptr<multiplexer>;

} // namespace net
} // namespace caf
