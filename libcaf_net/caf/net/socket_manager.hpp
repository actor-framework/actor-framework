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
#include "caf/net/operation.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/typed_actor_shell.hpp"
#include "caf/ref_counted.hpp"
#include "caf/tag/io_event_oriented.hpp"

namespace caf::net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class CAF_NET_EXPORT socket_manager : public ref_counted {
public:
  // -- member types -----------------------------------------------------------

  using fallback_handler = unique_callback_ptr<result<message>(message&)>;

  // -- constructors, destructors, and assignment operators --------------------

  /// @pre `handle != invalid_socket`
  /// @pre `parent != nullptr`
  socket_manager(socket handle, multiplexer* parent);

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
    return *parent_;
  }

  /// Returns the owning @ref multiplexer instance.
  const multiplexer& mpx() const noexcept {
    return *parent_;
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  multiplexer* mpx_ptr() noexcept {
    return parent_;
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  const multiplexer* mpx_ptr() const noexcept {
    return parent_;
  }

  /// Returns registered operations (read, write, or both).
  operation mask() const noexcept {
    return mask_;
  }

  /// Adds given flag(s) to the event mask.
  /// @returns `false` if `mask() | flag == mask()`, `true` otherwise.
  /// @pre `flag != operation::none`
  bool mask_add(operation flag) noexcept;

  /// Tries to clear given flag(s) from the event mask.
  /// @returns `false` if `mask() & ~flag == mask()`, `true` otherwise.
  /// @pre `flag != operation::none`
  bool mask_del(operation flag) noexcept;

  const error& abort_reason() const noexcept {
    return abort_reason_;
  }

  void abort_reason(error reason) noexcept {
    abort_reason_ = std::move(reason);
  }

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

  void register_reading();

  void register_writing();

  void shutdown_reading();

  void shutdown_writing();

  // -- pure virtual member functions ------------------------------------------

  virtual error init(const settings& config) = 0;

  /// Called whenever the socket received new data.
  virtual bool handle_read_event() = 0;

  /// Called whenever the socket is allowed to send data.
  virtual bool handle_write_event() = 0;

  /// Called when the remote side becomes unreachable due to an error.
  /// @param code The error code as reported by the operating system.
  virtual void handle_error(sec code) = 0;

  /// Restarts a socket manager that suspended reads. Calling this member
  /// function on active managers is a no-op.
  virtual void continue_reading() = 0;

protected:
  // -- member variables -------------------------------------------------------

  socket handle_;

  operation mask_;

  multiplexer* parent_;

  error abort_reason_;
};

template <class Protocol>
class socket_manager_impl : public socket_manager {
public:
  // -- member types -----------------------------------------------------------

  using output_tag = tag::io_event_oriented;

  using socket_type = typename Protocol::socket_type;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  socket_manager_impl(socket_type handle, multiplexer* mpx, Ts&&... xs)
    : socket_manager(handle, mpx), protocol_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- initialization ---------------------------------------------------------

  error init(const settings& config) override {
    CAF_LOG_TRACE("");
    if (auto err = nonblocking(handle(), true)) {
      CAF_LOG_ERROR("failed to set nonblocking flag in socket:" << err);
      return err;
    }
    return protocol_.init(static_cast<socket_manager*>(this), this, config);
  }

  // -- properties -------------------------------------------------------------

  /// Returns the managed socket.
  socket_type handle() const {
    return socket_cast<socket_type>(this->handle_);
  }

  // -- event callbacks --------------------------------------------------------

  bool handle_read_event() override {
    return protocol_.handle_read_event(this);
  }

  bool handle_write_event() override {
    return protocol_.handle_write_event(this);
  }

  void handle_error(sec code) override {
    abort_reason_ = code;
    return protocol_.abort(this, abort_reason_);
  }

  void continue_reading() override {
    return protocol_.continue_reading(this);
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
