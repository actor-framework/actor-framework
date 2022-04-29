// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/callback.hpp"
#include "caf/detail/infer_actor_shell_ptr_type.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/typed_actor_shell.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"

#include <type_traits>

namespace caf::net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class CAF_NET_EXPORT socket_manager : public ref_counted {
public:
  // -- member types -----------------------------------------------------------

  using event_handler_ptr = std::unique_ptr<socket_event_layer>;

  /// Stores manager-related flags in a single block.
  struct flags_t {
    bool read_closed : 1;
    bool write_closed : 1;
  };

  // -- constructors, destructors, and assignment operators --------------------

  /// @pre `handle != invalid_socket`
  /// @pre `mpx!= nullptr`
  socket_manager(multiplexer* mpx, socket handle, event_handler_ptr handler);

  ~socket_manager() override;

  socket_manager(const socket_manager&) = delete;

  socket_manager& operator=(const socket_manager&) = delete;

  // -- factories --------------------------------------------------------------

  static socket_manager_ptr make(multiplexer* mpx, socket handle,
                                 event_handler_ptr handler);

  template <class Handle = actor, class FallbackHandler>
  detail::infer_actor_shell_ptr_type<Handle>
  make_actor_shell(FallbackHandler f) {
    using ptr_type = detail::infer_actor_shell_ptr_type<Handle>;
    using impl_type = typename ptr_type::element_type;
    auto hdl = system().spawn<impl_type>(this);
    auto ptr = ptr_type{actor_cast<strong_actor_ptr>(std::move(hdl))};
    ptr->set_fallback(std::move(f));
    return ptr;
  }

  template <class Handle = actor>
  auto make_actor_shell() {
    auto f = [](abstract_actor_shell* self, message& msg) -> result<message> {
      self->quit(make_error(sec::unexpected_message, std::move(msg)));
      return make_error(sec::unexpected_message);
    };
    return make_actor_shell<Handle>(std::move(f));
  }

  /// Returns a thread-safe disposer for stopping the socket manager from an
  /// outside context.
  disposable make_disposer();

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

  /// Returns whether the manager closed read operations on the socket.
  [[nodiscard]] bool read_closed() const noexcept {
    return flags_.read_closed;
  }

  /// Returns whether the manager closed write operations on the socket.
  [[nodiscard]] bool write_closed() const noexcept {
    return flags_.write_closed;
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

  /// Deregisters the manager from read operations and blocks any future
  /// attempts to re-register it.
  void shutdown_read();

  /// Deregisters the manager from write operations and blocks any future
  /// attempts to re-register it.
  void shutdown_write();

  /// Deregisters the manager from both read and write operations and blocks any
  /// future attempts to re-register it.
  void shutdown();

  // -- callbacks for the handler ----------------------------------------------

  /// Schedules a call to `do_handover` on the handler.
  void schedule_handover();

  /// Schedules @p what to run later.
  void schedule(action what);

  /// Schedules @p what to run later.
  template <class F>
  void schedule_fn(F&& what) {
    schedule(make_action(std::forward<F>(what)));
  }

  // -- callbacks for the multiplexer ------------------------------------------

  /// Closes the read channel of the socket.
  void close_read() noexcept;

  /// Closes the write channel of the socket.
  void close_write() noexcept;

  /// Initializes the manager and its all of its sub-components.
  error init(const settings& cfg);

  /// Called whenever the socket received new data.
  void handle_read_event();

  /// Called whenever the socket is allowed to send data.
  void handle_write_event();

  /// Called when the remote side becomes unreachable due to an error.
  /// @param code The error code as reported by the operating system.
  void handle_error(sec code);

private:
  // -- utility functions ------------------------------------------------------

  socket_manager_ptr strong_this();

  // -- member variables -------------------------------------------------------

  /// Stores the socket file descriptor. The socket manager automatically closes
  /// the socket in its destructor.
  socket fd_;

  /// Points to the multiplexer that owns this manager.
  multiplexer* mpx_;

  /// Stores flags for the socket file descriptor.
  flags_t flags_;

  /// Stores the event handler that operators on the socket file descriptor.
  event_handler_ptr handler_;
};

/// @relates socket_manager
using socket_manager_ptr = intrusive_ptr<socket_manager>;

} // namespace caf::net
