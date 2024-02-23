// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/typed_actor_shell.hpp"

#include "caf/action.hpp"
#include "caf/actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/callback.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/infer_actor_shell_ptr_type.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/fwd.hpp"
#include "caf/sec.hpp"

#include <type_traits>

namespace caf::net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class CAF_NET_EXPORT socket_manager : public detail::atomic_ref_counted,
                                      public disposable_impl,
                                      public flow::coordinator {
public:
  // -- member types -----------------------------------------------------------

  using event_handler_ptr = std::unique_ptr<socket_event_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  /// @pre `handle != invalid_socket`
  /// @pre `mpx!= nullptr`
  socket_manager(multiplexer* mpx, event_handler_ptr handler);

  ~socket_manager() override;

  socket_manager(const socket_manager&) = delete;

  socket_manager& operator=(const socket_manager&) = delete;

  // -- factories --------------------------------------------------------------

  static socket_manager_ptr make(multiplexer* mpx, event_handler_ptr handler);

  // -- properties -------------------------------------------------------------

  /// Returns the handle for the managed socket.
  socket handle() const {
    return fd_;
  }

  /// @private
  void handle(socket new_handle) {
    fd_ = new_handle;
  }

  /// Returns a reference to the hosting @ref actor_system instance.
  actor_system& system() noexcept;

  /// Returns the owning @ref multiplexer instance.
  multiplexer& mpx() noexcept {
    return *mpx_;
  }

  /// Returns the owning @ref multiplexer instance.
  const multiplexer& mpx() const noexcept {
    return *mpx_;
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  multiplexer* mpx_ptr() noexcept {
    return mpx_;
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  const multiplexer* mpx_ptr() const noexcept {
    return mpx_;
  }

  /// Queries whether the manager is registered for reading.
  bool is_reading() const noexcept;

  /// Queries whether the manager is registered for writing.
  bool is_writing() const noexcept;

  // -- event loop management --------------------------------------------------

  /// Registers the manager for read operations.
  void register_reading();

  /// Registers the manager for write operations.
  void register_writing();

  /// Deregisters the manager from read operations.
  void deregister_reading();

  /// Deregisters the manager from write operations.
  void deregister_writing();

  /// Deregisters the manager from both read and write operations.
  void deregister();

  /// Schedules a call to `fn` on the multiplexer when this socket manager
  /// cleans up its state.
  /// @note Must be called before `start`.
  void add_cleanup_listener(action fn) {
    cleanup_listeners_.push_back(std::move(fn));
  }

  // -- callbacks for the handler ----------------------------------------------

  /// Schedules a call to `do_handover` on the handler.
  void schedule_handover();

  /// Shuts down this socket manager.
  void shutdown();

  // -- callbacks for the multiplexer ------------------------------------------

  /// Starts the manager and its all of its processing layers.
  error start();

  /// Called whenever the socket received new data.
  void handle_read_event();

  /// Called whenever the socket is allowed to send data.
  void handle_write_event();

  /// Called when the remote side becomes unreachable due to an error or after
  /// calling @ref dispose.
  /// @param code The error code as reported by the operating system or
  ///             @ref sec::disposed.
  void handle_error(sec code);

  // -- implementation of coordinator ------------------------------------------

  void ref_execution_context() const noexcept override;

  void deref_execution_context() const noexcept override;

  void schedule(action what) override;

  void watch(disposable what) override;

  void release_later(flow::coordinated_ptr& child) override;

  steady_time_point steady_time() override;

  void delay(action what) override;

  disposable delay_until(steady_time_point abs_time, action what) override;

  // -- implementation of disposable_impl --------------------------------------

  void dispose() override;

  bool disposed() const noexcept override;

  void ref_disposable() const noexcept override;

  void deref_disposable() const noexcept override;

  // -- disambiguation ---------------------------------------------------------

private:
  // -- utility functions ------------------------------------------------------

  void exec(action& f);

  void run_delayed_actions();

  void cleanup();

  socket_manager_ptr strong_this();

  // -- member variables -------------------------------------------------------

  /// Stores the socket file descriptor. The socket manager automatically closes
  /// the socket in its destructor.
  socket fd_;

  /// Points to the multiplexer that executes this manager. Note: we do not need
  /// to increase the reference count for the multiplexer, because the
  /// multiplexer owns all managers in the sense that calling any member
  /// function on a socket manager may not occur if the actor system has shut
  /// down (and the multiplexer is part of the actor system).
  multiplexer* mpx_;

  /// Stores the event handler that operators on the socket file descriptor.
  event_handler_ptr handler_;

  /// Stores whether `shutdown` has been called.
  bool shutting_down_ = false;

  /// Stores whether the manager has been either explicitly disposed or shut
  /// down by demand of the application.
  std::atomic<bool> disposed_;

  /// Callbacks to run when calling `cleanup`.
  std::vector<action> cleanup_listeners_;

  /// Stores watched disposables.
  std::vector<disposable> watched_;

  /// Stores actions that should run at the next opportunity.
  std::deque<action> delayed_;

  /// Stores flow children that should be released at the next opportunity.
  std::vector<flow::coordinated_ptr> trash_;
};

// -- related free functions ---------------------------------------------------

/// @relates socket_manager
CAF_NET_EXPORT void intrusive_ptr_add_ref(socket_manager* ptr) noexcept;

/// @relates socket_manager
CAF_NET_EXPORT void intrusive_ptr_release(socket_manager* ptr) noexcept;

// -- related types ------------------------------------------------------------

/// @relates socket_manager
using socket_manager_ptr = intrusive_ptr<socket_manager>;

} // namespace caf::net
