/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/make_counted.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/operation.hpp"
#include "caf/net/socket.hpp"
#include "caf/ref_counted.hpp"

namespace caf::net {

/// Manages the lifetime of a single socket and handles any I/O events on it.
class CAF_NET_EXPORT socket_manager : public ref_counted {
public:
  // -- constructors, destructors, and assignment operators --------------------

  /// @pre `handle != invalid_socket`
  /// @pre `parent != nullptr`
  socket_manager(socket handle, multiplexer* parent);

  ~socket_manager() override;

  socket_manager(const socket_manager&) = delete;

  socket_manager& operator=(const socket_manager&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the managed socket.
  template <class Socket = socket>
  Socket handle() const {
    return socket_cast<Socket>(handle_);
  }

  /// Returns the owning @ref multiplexer instance.
  multiplexer& mpx() noexcept {
    return *parent_;
  }

  /// Returns the owning @ref multiplexer instance.
  const multiplexer& mpx() const noexcept {
    return *parent_;
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

  // -- event loop management --------------------------------------------------

  void register_reading();

  void register_writing();

  // -- pure virtual member functions ------------------------------------------

  virtual error init(const settings& config) = 0;

  /// Called whenever the socket received new data.
  virtual bool handle_read_event() = 0;

  /// Called whenever the socket is allowed to send data.
  virtual bool handle_write_event() = 0;

  /// Called when the remote side becomes unreachable due to an error.
  /// @param code The error code as reported by the operating system.
  virtual void handle_error(sec code) = 0;

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
  template <class... Ts>
  socket_manager_impl(typename Protocol::socket_type handle, multiplexer* mpx,
                      Ts&&... xs)
    : socket_manager{handle, mpx}, protocol_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- initialization ---------------------------------------------------------

  error init(const settings& config) override {
    return protocol_.init(static_cast<socket_manager*>(this), *this, config);
  }

  // -- event callbacks --------------------------------------------------------

  bool handle_read_event() override {
    return protocol_.handle_read_event(*this);
  }

  bool handle_write_event() override {
    return protocol_.handle_write_event(*this);
  }

  void handle_error(sec code) override {
    abort_reason_ = code;
    return protocol_.abort(*this, abort_reason_);
  }

  auto& protocol() noexcept {
    return protocol_;
  }

  const auto& protocol() const noexcept {
    return protocol_;
  }

private:
  Protocol protocol_;
};

/// @relates socket_manager
using socket_manager_ptr = intrusive_ptr<socket_manager>;

template <class B, template <class> class... Layers>
struct socket_type_helper;

template <class B>
struct socket_type_helper<B> {
  using type = typename B::socket_type;
};

template <class B, template <class> class Layer,
          template <class> class... Layers>
struct socket_type_helper<B, Layer, Layers...>
  : socket_type_helper<Layer<B>, Layers...> {
  using upper_input = typename B::input_tag;
  using lower_output = typename Layer<B>::output_tag;
  static_assert(std::is_same<upper_input, lower_output>::value);
};

template <class B, template <class> class... Layers>
using socket_type_t = typename socket_type_helper<B, Layers...>::type;

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
  // no content
};

template <class B, template <class> class... Layers>
using make_socket_manager_helper_t =
  typename make_socket_manager_helper<B, Layers...>::type;

template <class App, template <class> class... Layers, class... Ts>
auto make_socket_manager(socket_type_t<App, Layers...> handle, multiplexer* mpx,
                         Ts&&... xs) {
  static_assert(std::is_base_of<socket, socket_type_t<App, Layers...>>::value);
  using impl
    = make_socket_manager_helper_t<App, Layers..., socket_manager_impl>;
  return make_counted<impl>(std::move(handle), mpx, std::forward<Ts>(xs)...);
}

} // namespace caf::net
