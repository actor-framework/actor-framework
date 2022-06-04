// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

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

  using read_result = socket_event_layer::read_result;

  using write_result = socket_event_layer::write_result;

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

  // -- event loop management --------------------------------------------------

  /// Registers the manager for read operations on the @ref multiplexer.
  /// @thread-safe
  void register_reading();

  /// Registers the manager for write operations on the @ref multiplexer.
  /// @thread-safe
  void register_writing();

  /// Schedules a call to `handle_continue_reading` on the @ref multiplexer.
  /// This mechanism allows users to signal changes in the environment to the
  /// manager that allow it to make progress, e.g., new demand in asynchronous
  /// buffer that allow the manager to push available data downstream. The event
  /// is a no-op if the manager is already registered for reading.
  /// @thread-safe
  void continue_reading();

  /// Schedules a call to `handle_continue_reading` on the @ref multiplexer.
  /// This mechanism allows users to signal changes in the environment to the
  /// manager that allow it to make progress, e.g., new data for writing in an
  /// asynchronous buffer. The event is a no-op if the manager is already
  /// registered for writing.
  /// @thread-safe
  void continue_writing();

  // -- callbacks for the multiplexer ------------------------------------------

  /// Closes the read channel of the socket.
  void close_read() noexcept;

  /// Closes the write channel of the socket.
  void close_write() noexcept;

  /// Initializes the manager and its all of its sub-components.
  error init(const settings& cfg);

  /// Called whenever the socket received new data.
  read_result handle_read_event();

  /// Called after handovers to allow the manager to process any data that is
  /// already buffered at the transport policy and thus would not trigger a read
  /// event on the socket.
  read_result handle_buffered_data();

  /// Restarts a socket manager that suspended reads. Calling this member
  /// function on active managers is a no-op. This function also should read any
  /// data buffered outside of the socket.
  read_result handle_continue_reading();

  /// Called whenever the socket is allowed to send data.
  write_result handle_write_event();

  /// Called when the remote side becomes unreachable due to an error.
  /// @param code The error code as reported by the operating system.
  void handle_error(sec code);

  /// Performs a handover to another transport after `handle_read_event` or
  /// `handle_read_event` returned `handover`.
  [[nodiscard]] bool do_handover();

private:
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
