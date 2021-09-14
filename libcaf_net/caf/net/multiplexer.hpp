// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

  /// Schedules a call to `mgr->handle_error(sec::discarded)`.
  /// @thread-safe
  void discard(const socket_manager_ptr& mgr);

  /// Stops further reading by `mgr`.
  /// @thread-safe
  void shutdown_reading(const socket_manager_ptr& mgr);

  /// Stops further writing by `mgr`.
  /// @thread-safe
  void shutdown_writing(const socket_manager_ptr& mgr);

  /// Schedules an action for execution on this multiplexer.
  /// @thread-safe
  void schedule(const action& what);

  /// Schedules an action for execution on this multiplexer.
  /// @thread-safe
  template <class F>
  void schedule_fn(F f) {
    schedule(make_action(std::move(f)));
  }

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

private:
  /// Writes `opcode` and pointer to `mgr` the the pipe for handling an event
  /// later via the pollset updater.
  template <class T>
  void write_to_pipe(uint8_t opcode, T* ptr);

  template <class Enum, class T>
  std::enable_if_t<std::is_enum_v<Enum>> write_to_pipe(Enum opcode, T* ptr) {
    write_to_pipe(static_cast<uint8_t>(opcode), ptr);
  }
};

} // namespace caf::net
