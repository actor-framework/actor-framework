// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket.hpp"

#include "caf/action.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/ref_counted.hpp"
#include "caf/unordered_flat_map.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace caf::net {

/// Multiplexes any number of ::socket_manager objects with a ::socket.
class CAF_NET_EXPORT multiplexer_impl : public multiplexer {
public:
  // -- factories --------------------------------------------------------------

  /// @param parent Points to the owning middleman instance. May be `nullptr`
  ///               only for the purpose of unit testing if no @ref
  ///               socket_manager requires access to the @ref middleman or the
  ///               @ref actor_system.
  static multiplexer_ptr make(middleman* parent) {
    auto ptr = new multiplexer_impl(parent);
    return multiplexer_ptr{ptr, false};
  }

  // -- initialization ---------------------------------------------------------

  error init() override;

  // -- properties -------------------------------------------------------------

  /// Returns the number of currently active socket managers.
  size_t num_socket_managers() const noexcept override;

  /// Returns the index of `mgr` in the pollset or `-1`.
  ptrdiff_t index_of(const socket_manager_ptr& mgr) const noexcept override;

  /// Returns the index of `fd` in the pollset or `-1`.
  ptrdiff_t index_of(socket fd) const noexcept override;

  /// Returns the owning @ref middleman instance.
  middleman& owner() override;

  /// Returns the enclosing @ref actor_system.
  actor_system& system() override;

  // -- implementation of execution_context ------------------------------------

  void ref_execution_context() const noexcept override;

  void deref_execution_context() const noexcept override;

  void schedule(action what) override;

  void watch(disposable what) override;

  // -- thread-safe signaling --------------------------------------------------

  /// Registers `mgr` for initialization in the multiplexer's thread.
  /// @thread-safe
  void start(socket_manager_ptr mgr) override;

  /// Signals the multiplexer to initiate shutdown.
  /// @thread-safe
  void shutdown() override;

  // -- callbacks for socket managers ------------------------------------------

  /// Registers `mgr` for read events.
  void register_reading(socket_manager* mgr) override;

  /// Registers `mgr` for write events.
  void register_writing(socket_manager* mgr) override;

  /// Deregisters `mgr` from read events.
  void deregister_reading(socket_manager* mgr) override;

  /// Deregisters `mgr` from write events.
  void deregister_writing(socket_manager* mgr) override;

  /// Deregisters @p mgr from read and write events.
  void deregister(socket_manager* mgr) override;

  /// Queries whether `mgr` is currently registered for reading.
  bool is_reading(const socket_manager* mgr) const noexcept override;

  /// Queries whether `mgr` is currently registered for writing.
  bool is_writing(const socket_manager* mgr) const noexcept override;

  // -- control flow -----------------------------------------------------------

  /// Polls I/O activity once and runs all socket event handlers that become
  /// ready as a result.
  bool poll_once(bool blocking) override;

  /// Runs `poll_once(false)` until it returns `true`.`
  void poll() override;

  /// Applies all pending updates.
  void apply_updates() override;

  /// Runs all pending actions.
  // void run_actions() override;

  /// Sets the thread ID to `std::this_thread::id()`.
  void set_thread_id() override;

  /// Runs the multiplexer until no socket event handler remains active.
  void run() override;

protected:
  // -- utility functions ------------------------------------------------------

  /// Handles an I/O event on given manager.
  void handle(const socket_manager_ptr& mgr, short events, short revents);

  /// Returns a change entry for the socket at given index. Lazily creates a new
  /// entry before returning if necessary.
  poll_update& update_for(ptrdiff_t index);

  /// Returns a change entry for the socket of the manager.
  poll_update& update_for(socket_manager* mgr);

  /// Writes `opcode` and pointer to `mgr` the the pipe for handling an event
  /// later via the pollset updater.
  /// @warning assumes ownership of @p ptr.
  template <class T>
  void write_to_pipe(uint8_t opcode, T* ptr);

  /// @copydoc write_to_pipe
  template <class Enum, class T>
  std::enable_if_t<std::is_enum_v<Enum>> write_to_pipe(Enum opcode, T* ptr) {
    write_to_pipe(static_cast<uint8_t>(opcode), ptr);
  }

  /// Queries the currently active event bitmask for `mgr`.
  short active_mask_of(const socket_manager* mgr) const noexcept;

  // -- member variables -------------------------------------------------------

  /// Bookkeeping data for managed sockets.
  pollfd_list pollset_;

  /// Maps sockets to their owning managers by storing the managers in the same
  /// order as their sockets appear in `pollset_`.
  manager_list managers_;

  /// Caches changes to the events mask of managed sockets until they can safely
  /// take place.
  poll_update_map updates_;

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

  /// Pending actions via `schedule`.
  std::deque<action> pending_actions_;

  /// Keeps track of watched disposables.
  std::vector<disposable> watched_;

private:
  // -- constructors, destructors, and assignment operators --------------------

  explicit multiplexer_impl(middleman* parent);

  // -- internal getter for the pollset updater --------------------------------

  std::deque<action>& pending_actions() override;

  // -- internal callbacks the pollset updater ---------------------------------

  void do_shutdown() override;

  void do_start(const socket_manager_ptr& mgr) override;
};

} // namespace caf::net
