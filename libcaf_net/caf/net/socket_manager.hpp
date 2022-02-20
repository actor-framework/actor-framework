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
#include "caf/make_counted.hpp"
#include "caf/net/actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/typed_actor_shell.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/tag/io_event_oriented.hpp"

namespace caf::net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class CAF_NET_EXPORT socket_manager : public ref_counted {
public:
  // -- member types -----------------------------------------------------------

  /// A callback for unprocessed messages.
  using fallback_handler = unique_callback_ptr<result<message>(message&)>;

  /// Encodes how a manager wishes to proceed after a read operation.
  enum class read_result {
    /// Indicates that a manager wants to read again later.
    again,
    /// Indicates that a manager wants to stop reading until explicitly resumed.
    stop,
    /// Indicates that a manager wants to write to the socket instead of reading
    /// from the socket.
    want_write,
    /// Indicates that a manager is done with the socket and hands ownership to
    /// another manager.
    handover,
  };

  /// Encodes how a manager wishes to proceed after a write operation.
  enum class write_result {
    /// Indicates that a manager wants to read again later.
    again,
    /// Indicates that a manager wants to stop reading until explicitly resumed.
    stop,
    /// Indicates that a manager wants to read from the socket instead of
    /// writing to the socket.
    want_read,
    /// Indicates that a manager is done with the socket and hands ownership to
    /// another manager.
    handover,
  };

  /// Stores manager-related flags in a single block.
  struct flags_t {
    bool read_closed : 1;
    bool write_closed : 1;
  };

  // -- constructors, destructors, and assignment operators --------------------

  /// @pre `handle != invalid_socket`
  /// @pre `mpx!= nullptr`
  socket_manager(socket handle, multiplexer* mpx);

  ~socket_manager() override;

  socket_manager(const socket_manager&) = delete;

  socket_manager& operator=(const socket_manager&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the managed socket.
  socket handle() const {
    return handle_;
  }

  /// @private
  void handle(socket new_handle) {
    handle_ = new_handle;
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

  /// Closes the read channel of the socket.
  void close_read() noexcept;

  /// Closes the write channel of the socket.
  void close_write() noexcept;

  /// Returns whether the manager closed read operations on the socket.
  [[nodiscard]] bool read_closed() const noexcept {
    return flags_.read_closed;
  }

  /// Returns whether the manager closed write operations on the socket.
  [[nodiscard]] bool write_closed() const noexcept {
    return flags_.write_closed;
  }

  const error& abort_reason() const noexcept {
    return abort_reason_;
  }

  void abort_reason(error reason) noexcept;

  template <class... Ts>
  const error& abort_reason_or(Ts&&... xs) {
    if (!abort_reason_)
      abort_reason_ = make_error(std::forward<Ts>(xs)...);
    return abort_reason_;
  }

  // -- factories --------------------------------------------------------------

  template <class Handle = actor, class LowerLayerPtr, class FallbackHandler>
  auto make_actor_shell(LowerLayerPtr, FallbackHandler f) {
    using ptr_type = detail::infer_actor_shell_ptr_type<Handle>;
    using impl_type = typename ptr_type::element_type;
    auto hdl = system().spawn<impl_type>(this);
    auto ptr = ptr_type{actor_cast<strong_actor_ptr>(std::move(hdl))};
    ptr->set_fallback(std::move(f));
    return ptr;
  }

  template <class Handle = actor, class LowerLayerPtr>
  auto make_actor_shell(LowerLayerPtr down) {
    auto f = [down](message& msg) -> result<message> {
      down->abort_reason(make_error(sec::unexpected_message, std::move(msg)));
      return make_error(sec::unexpected_message);
    };
    return make_actor_shell<Handle>(down, std::move(f));
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

  /// Performs a handover to another manager after `handle_read_event` or
  /// `handle_read_event` returned `handover`.
  socket_manager_ptr do_handover();

  // -- pure virtual member functions ------------------------------------------

  /// Initializes the manager and its all of its sub-components.
  virtual error init(const settings& config) = 0;

  /// Called whenever the socket received new data.
  virtual read_result handle_read_event() = 0;

  /// Called after handovers to allow the manager to process any data that is
  /// already buffered at the transport policy and thus would not trigger a read
  /// event on the socket.
  virtual read_result handle_buffered_data() = 0;

  /// Restarts a socket manager that suspended reads. Calling this member
  /// function on active managers is a no-op. This function also should read any
  /// data buffered outside of the socket.
  virtual read_result handle_continue_reading() = 0;

  /// Called whenever the socket is allowed to send data.
  virtual write_result handle_write_event() = 0;

  /// Restarts a socket manager that suspended writes. Calling this member
  /// function on active managers is a no-op.
  virtual write_result handle_continue_writing() = 0;

  /// Called when the remote side becomes unreachable due to an error.
  /// @param code The error code as reported by the operating system.
  virtual void handle_error(sec code) = 0;

  /// Returns the new manager for the socket after `handle_read_event` or
  /// `handle_read_event` returned `handover`.
  /// @note Called from `do_handover`.
  /// @note When returning a non-null pointer, the new manager *must* be
  ///       initialized.
  virtual socket_manager_ptr make_next_manager(socket handle);

protected:
  // -- protected member variables ---------------------------------------------

  socket handle_;

  multiplexer* mpx_;

private:
  // -- private member variables -----------------------------------------------

  error abort_reason_;

  flags_t flags_;
};

template <class Protocol>
class socket_manager_impl : public socket_manager {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  using output_tag = tag::io_event_oriented;

  using socket_type = typename Protocol::socket_type;

  using read_result = typename super::read_result;

  using write_result = typename super::write_result;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  socket_manager_impl(socket_type handle, multiplexer* mpx, Ts&&... xs)
    : socket_manager(handle, mpx), protocol_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns the managed socket.
  socket_type handle() const {
    return socket_cast<socket_type>(this->handle_);
  }

  auto& protocol() noexcept {
    return protocol_;
  }

  const auto& protocol() const noexcept {
    return protocol_;
  }

  auto& top_layer() noexcept {
    return climb(protocol_);
  }

  const auto& top_layer() const noexcept {
    return climb(protocol_);
  }

  // -- interface functions ----------------------------------------------------

  error init(const settings& config) override {
    CAF_LOG_TRACE("");
    if (auto err = nonblocking(handle(), true)) {
      CAF_LOG_ERROR("failed to set nonblocking flag in socket:" << err);
      return err;
    }
    return protocol_.init(static_cast<socket_manager*>(this), this, config);
  }

  read_result handle_read_event() override {
    return protocol_.handle_read_event(this);
  }

  read_result handle_buffered_data() override {
    return protocol_.handle_buffered_data(this);
  }

  read_result handle_continue_reading() override {
    return protocol_.handle_continue_reading(this);
  }

  write_result handle_write_event() override {
    return protocol_.handle_write_event(this);
  }

  write_result handle_continue_writing() override {
    return protocol_.handle_continue_writing(this);
  }

  void handle_error(sec code) override {
    this->abort_reason(make_error(code));
    return protocol_.abort(this, this->abort_reason());
  }

private:
  template <class FinalLayer>
  static FinalLayer& climb(FinalLayer& layer) {
    return layer;
  }

  template <class FinalLayer>
  static const FinalLayer& climb(const FinalLayer& layer) {
    return layer;
  }

  template <template <class> class Layer, class Next>
  static auto& climb(Layer<Next>& layer) {
    return climb(layer.upper_layer());
  }

  template <template <class> class Layer, class Next>
  static const auto& climb(const Layer<Next>& layer) {
    return climb(layer.upper_layer());
  }

  Protocol protocol_;
};

/// @relates socket_manager
using socket_manager_ptr = intrusive_ptr<socket_manager>;

template <class B, template <class> class... Layers>
struct make_socket_manager_helper;

template <class B>
struct make_socket_manager_helper<B> {
  using type = B;
};

template <class B, template <class> class Layer,
          template <class> class... Layers>
struct make_socket_manager_helper<B, Layer, Layers...>
  : make_socket_manager_helper<Layer<B>, Layers...> {
  using upper_input = typename B::input_tag;
  using lower_output = typename Layer<B>::output_tag;
  static_assert(std::is_same<upper_input, lower_output>::value);
};

template <class B, template <class> class... Layers>
using make_socket_manager_helper_t =
  typename make_socket_manager_helper<B, Layers...>::type;

template <class B, template <class> class... Layers>
using socket_type_t =
  typename make_socket_manager_helper_t<B, Layers...,
                                        socket_manager_impl>::socket_type;

template <class App, template <class> class... Layers, class... Ts>
auto make_socket_manager(socket_type_t<App, Layers...> handle, multiplexer* mpx,
                         Ts&&... xs) {
  using impl
    = make_socket_manager_helper_t<App, Layers..., socket_manager_impl>;
  static_assert(std::is_base_of<socket, typename impl::socket_type>::value);
  return make_counted<impl>(std::move(handle), mpx, std::forward<Ts>(xs)...);
}

} // namespace caf::net
