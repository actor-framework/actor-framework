// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/fwd.hpp"
#include "caf/sec.hpp"

#include <memory>
#include <type_traits>

namespace caf::net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class CAF_NET_EXPORT socket_manager : public detail::atomic_ref_counted,
                                      public disposable_impl,
                                      public flow::coordinator {
public:
  // -- friends ----------------------------------------------------------------

  friend class abstract_actor_shell;

  // -- member types -----------------------------------------------------------

  using event_handler_ptr = std::unique_ptr<socket_event_layer>;

  // -- factories --------------------------------------------------------------

  static socket_manager_ptr make(multiplexer* mpx, event_handler_ptr handler);

  // -- properties -------------------------------------------------------------

  /// Returns the handle for the managed socket.
  virtual socket handle() const = 0;

  /// Returns the owning @ref multiplexer instance.
  multiplexer& mpx() const {
    CAF_ASSERT(mpx_ptr());
    return *mpx_ptr();
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  virtual multiplexer* mpx_ptr() const noexcept = 0;

  /// Queries whether the manager is registered for reading.
  virtual bool is_reading() const noexcept = 0;

  /// Queries whether the manager is registered for writing.
  virtual bool is_writing() const noexcept = 0;

  // -- event loop management --------------------------------------------------

  /// Registers the manager for read operations.
  virtual void register_reading() = 0;

  /// Registers the manager for write operations.
  virtual void register_writing() = 0;

  /// Deregisters the manager from read operations.
  virtual void deregister_reading() = 0;

  /// Deregisters the manager from write operations.
  virtual void deregister_writing() = 0;

  /// Deregisters the manager from both read and write operations.
  virtual void deregister() = 0;

  /// Schedules a call to `fn` on the multiplexer when this socket manager
  /// cleans up its state.
  /// @note Must be called before `start`.
  virtual void add_cleanup_listener(action fn) = 0;

  // -- callbacks for the handler ----------------------------------------------

  /// Schedules a call to `do_handover` on the handler.
  virtual void schedule_handover() = 0;

  /// Shuts down this socket manager.
  virtual void shutdown() = 0;

  // -- callbacks for the multiplexer ------------------------------------------

  /// Starts the manager and its all of its processing layers.
  virtual error start() = 0;

  /// Called whenever the socket received new data.
  virtual void handle_read_event() = 0;

  /// Called whenever the socket is allowed to send data.
  virtual void handle_write_event() = 0;

  /// Called when the remote side becomes unreachable due to an error or after
  /// calling @ref dispose.
  /// @param code The error code as reported by the operating system or
  ///             @ref sec::disposed.
  virtual void handle_error(sec code) = 0;

private:
  virtual void run_delayed_actions() = 0;
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
