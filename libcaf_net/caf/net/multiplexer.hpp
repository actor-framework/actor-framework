// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/socket.hpp"

#include "caf/async/execution_context.hpp"
#include "caf/detail/atomic_ref_counted.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace caf::net {

/// Multiplexes any number of ::socket_manager objects with a ::socket.
class CAF_NET_EXPORT multiplexer : public detail::atomic_ref_counted,
                                   public async::execution_context {
public:
  // -- static utility functions -----------------------------------------------

  /// Blocks the PIPE signal on the current thread when running on a POSIX
  /// windows. Has no effect when running on Windows.
  static void block_sigpipe();

  /// Returns a pointer to the multiplexer from the actor system.
  static multiplexer* from(actor_system& sys);

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~multiplexer();

  // -- factories --------------------------------------------------------------

  /// @param parent Points to the owning middleman instance. May be `nullptr`
  ///               only for the purpose of unit testing if no @ref
  ///               socket_manager requires access to the @ref middleman or the
  ///               @ref actor_system.
  static multiplexer_ptr make_default(middleman* parent);

  // -- initialization ---------------------------------------------------------

  virtual error init() = 0;

  // -- properties -------------------------------------------------------------

  /// Returns the number of currently active socket managers.
  virtual size_t num_socket_managers() const noexcept = 0;

  /// Returns the index of `mgr` in the pollset or `-1`.
  virtual ptrdiff_t index_of(const socket_manager_ptr& mgr) const noexcept = 0;

  /// Returns the index of `fd` in the pollset or `-1`.
  virtual ptrdiff_t index_of(socket fd) const noexcept = 0;

  /// Returns the owning @ref middleman instance.
  virtual middleman& owner() = 0;

  /// Returns the enclosing @ref actor_system.
  virtual actor_system& system() = 0;

  // -- thread-safe signaling --------------------------------------------------

  /// Registers `mgr` for initialization in the multiplexer's thread.
  /// @thread-safe
  virtual void start(socket_manager_ptr mgr) = 0;

  /// Signals the multiplexer to initiate shutdown.
  /// @thread-safe
  virtual void shutdown() = 0;

  // -- callbacks for socket managers ------------------------------------------

  /// Registers `mgr` for read events.
  virtual void register_reading(socket_manager* mgr) = 0;

  /// Registers `mgr` for write events.
  virtual void register_writing(socket_manager* mgr) = 0;

  /// Deregisters `mgr` from read events.
  virtual void deregister_reading(socket_manager* mgr) = 0;

  /// Deregisters `mgr` from write events.
  virtual void deregister_writing(socket_manager* mgr) = 0;

  /// Deregisters @p mgr from read and write events.
  virtual void deregister(socket_manager* mgr) = 0;

  /// Queries whether `mgr` is currently registered for reading.
  virtual bool is_reading(const socket_manager* mgr) const noexcept = 0;

  /// Queries whether `mgr` is currently registered for writing.
  virtual bool is_writing(const socket_manager* mgr) const noexcept = 0;

  // -- control flow -----------------------------------------------------------

  /// Polls I/O activity once and runs all socket event handlers that become
  /// ready as a result.
  virtual bool poll_once(bool blocking) = 0;

  /// Runs `poll_once(false)` until it returns `true`.`
  virtual void poll() = 0;

  /// Applies all pending updates.
  virtual void apply_updates() = 0;

  /// Runs all pending actions.
  // virtual void run_actions() = 0;

  /// Sets the thread ID to `std::this_thread::id()`.
  virtual void set_thread_id() = 0;

  /// Runs the multiplexer until no socket event handler remains active.
  virtual void run() = 0;
};

} // namespace caf::net
