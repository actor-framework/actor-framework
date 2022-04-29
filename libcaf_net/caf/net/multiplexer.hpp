// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>
#include <mutex>
#include <thread>

#include "caf/action.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/operation.hpp"
#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/ref_counted.hpp"
#include "caf/unordered_flat_map.hpp"

extern "C" {

struct pollfd;

} // extern "C"

namespace caf::net {

class pollset_updater;

/// Multiplexes any number of ::socket_manager objects with a ::socket.
class CAF_NET_EXPORT multiplexer {
public:
  // -- member types -----------------------------------------------------------

  struct poll_update {
    short events = 0;
    socket_manager_ptr mgr;
  };

  using poll_update_map = unordered_flat_map<socket, poll_update>;

  using pollfd_list = std::vector<pollfd>;

  using manager_list = std::vector<socket_manager_ptr>;

  // -- friends ----------------------------------------------------------------

  friend class pollset_updater; // Needs access to the `do_*` functions.

  // -- static utility functions -----------------------------------------------

  /// Blocks the PIPE signal on the current thread when running on a POSIX
  /// windows. Has no effect when running on Windows.
  static void block_sigpipe();

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
  ptrdiff_t index_of(const socket_manager_ptr& mgr) const noexcept;

  /// Returns the index of `fd` in the pollset or `-1`.
  ptrdiff_t index_of(socket fd) const noexcept;

  /// Returns the owning @ref middleman instance.
  middleman& owner();

  /// Returns the enclosing @ref actor_system.
  actor_system& system();

  /// Computes the current mask for the manager. Mostly useful for testing.
  operation mask_of(const socket_manager_ptr& mgr);

  // -- thread-safe signaling --------------------------------------------------

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

  /// Signals the multiplexer to initiate shutdown.
  /// @thread-safe
  void shutdown();

  // -- callbacks for socket managers ------------------------------------------

  /// Registers `mgr` for read events.
  void register_reading(socket_manager* mgr);

  /// Registers `mgr` for write events.
  void register_writing(socket_manager* mgr);

  /// Deregisters `mgr` from read events.
  void deregister_reading(socket_manager* mgr);

  /// Deregisters `mgr` from write events.
  void deregister_writing(socket_manager* mgr);

  /// Deregisters @p mgr from read and write events.
  void deregister(socket_manager* mgr);

  /// Queries whether `mgr` is currently registered for reading.
  bool is_reading(const socket_manager* mgr) const noexcept;

  /// Queries whether `mgr` is currently registered for writing.
  bool is_writing(const socket_manager* mgr) const noexcept;

  // -- control flow -----------------------------------------------------------

  /// Polls I/O activity once and runs all socket event handlers that become
  /// ready as a result.
  bool poll_once(bool blocking);

  /// Runs `poll_once(false)` until it returns `true`.`
  void poll();

  /// Applies all pending updates.
  void apply_updates();

  /// Sets the thread ID to `std::this_thread::id()`.
  void set_thread_id();

  /// Runs the multiplexer until no socket event handler remains active.
  void run();

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

private:
  // -- internal callbacks the pollset updater ---------------------------------

  void do_shutdown();

  void do_register_reading(const socket_manager_ptr& mgr);

  void do_discard(const socket_manager_ptr& mgr);

  void do_shutdown_reading(const socket_manager_ptr& mgr);

  void do_shutdown_writing(const socket_manager_ptr& mgr);

  void do_init(const socket_manager_ptr& mgr);
};

} // namespace caf::net
