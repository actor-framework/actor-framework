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

#include "caf/detail/overload.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/defaults.hpp"
#include "caf/net/receive_policy.hpp"

namespace caf::net {

/// Implements base class for transports.
/// @tparam Transport The actual type of the implementin transport.
/// @tparam NextLayer The Following Layer. Either `transport_worker` or
/// `transport_worker_dispatcher`
/// @tparam Handle The type of the used socket_handle
/// @tparam Application The type of the application used in this stack.
/// @tparam IdType The type of the Id used by this transport.
template <class Transport, class NextLayer, class Handle, class Application,
          class IdType>
class transport_base {
public:
  // -- member types -----------------------------------------------------------

  using next_layer_type = NextLayer;

  using handle_type = Handle;

  using transport_type = Transport;

  using application_type = Application;

  using id_type = IdType;

  using buffer_type = std::vector<byte>;

  using buffer_cache_type = std::vector<buffer_type>;

  // -- constructors, destructors, and assignment operators --------------------

  transport_base(handle_type handle, application_type application)
    : next_layer_(std::move(application)), handle_(handle), manager_(nullptr) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// @return the `socket_handle` of this transport.
  handle_type handle() const noexcept {
    return handle_;
  }

  /// @return reference to the `actor_system` of this transport.
  /// @pre `init` must be called before calling this getter.
  actor_system& system() {
    return manager().system();
  }

  /// @return reference to the `application` of this transport.
  application_type& application() {
    return next_layer_.application();
  }

  /// @return reference to `this` transport.
  transport_type& transport() {
    return *reinterpret_cast<transport_type*>(this);
  }

  /// @return reference to the `endpoint_manager` of this transport.
  /// @pre `init` must be called before calling this getter.
  endpoint_manager& manager() {
    CAF_ASSERT(manager_);
    return *manager_;
  }

  // -- transport member functions ---------------------------------------------

  /// Initializes this transport.
  /// @param parent the endpoint manager that manages this transport.
  /// @return `error` on failure, none on success.
  virtual error init(endpoint_manager& parent) {
    CAF_LOG_TRACE("");
    manager_ = &parent;
    auto& cfg = system().config();
    auto max_header_bufs = get_or(cfg, "middleman.max-header-buffers",
                                  defaults::middleman::max_header_buffers);
    header_bufs_.reserve(max_header_bufs);
    auto max_payload_bufs = get_or(cfg, "middleman.max-payload-buffers",
                                   defaults::middleman::max_payload_buffers);
    payload_bufs_.reserve(max_payload_bufs);
    if (auto err = next_layer_.init(*this))
      return err;
    return none;
  }

  /// Resolves a remote actor using 'locator' and sends the resolved actor to
  /// listener on success - an error otherwise.
  /// @param locator The `uri` of the remote actor.
  /// @param listener actor_handle which the resolved actor should be sent to.
  auto resolve(endpoint_manager&, const uri& locator, const actor& listener) {
    CAF_LOG_TRACE(CAF_ARG(locator) << CAF_ARG(listener));
    auto f = detail::make_overload(
      [&](auto& layer) -> decltype(layer.resolve(*this, locator, listener)) {
        return layer.resolve(*this, locator, listener);
      },
      [&](auto& layer) -> decltype(
                         layer.resolve(*this, locator.path(), listener)) {
        return layer.resolve(*this, locator.path(), listener);
      });
    f(next_layer_);
  }

  ///
  void new_proxy(endpoint_manager&, const node_id& peer, actor_id id) {
    next_layer_.new_proxy(*this, peer, id);
  }

  /// Notifies the remote endpoint that the local actor is down.
  /// @param peer The `node_id` of the remote endpoint.
  /// @param id The `actor_id` of the remote actor.
  /// @param reason The reason why the local actor has shut_down.
  void local_actor_down(endpoint_manager&, const node_id& peer, actor_id id,
                        error reason) {
    next_layer_.local_actor_down(*this, peer, id, std::move(reason));
  }

  /// timeout callback for this transport. Will be called after a timeout is
  /// triggered.
  /// @param value The `atom_value` of the timeout.
  /// @param id The timeout id of this timeout.
  void timeout(endpoint_manager&, atom_value value, uint64_t id) {
    next_layer_.timeout(*this, value, id);
  }

  /// Callback for setting a timeout. Will be called after setting a timeout to
  /// get the timeout id for local use.
  /// @param timeout_id The timeout_id of the previously set timeout.
  /// @param ts Any further information that was passed when setting the
  /// timeout.
  template <class... Ts>
  void set_timeout(uint64_t timeout_id, Ts&&... ts) {
    next_layer_.set_timeout(timeout_id, std::forward<Ts>(ts)...);
  }

  /// Callback for when an error occurs.
  /// @param code The error code to handle.
  void handle_error(sec code) {
    next_layer_.handle_error(code);
  }

  // -- (pure) virtual functions -----------------------------------------------

  /// Configures the next read_event.
  virtual void configure_read(receive_policy::config){
    // nop
  };

  /// Callback when a read_event occurs.
  virtual bool handle_read_event(endpoint_manager&) = 0;

  /// Callback when a write_event occurs.
  virtual bool handle_write_event(endpoint_manager& parent) = 0;

  /// Moves a packet that is scattered across multiple buffers into the
  /// write_queue of this transport.
  /// @param id the endpoint id
  /// @param buffers the packet scattered across multiple buffers.
  virtual void write_packet(id_type id, span<buffer_type*> buffers) = 0;

  // -- buffer management ------------------------------------------------------

  /// Returns the next cached header_buffer or creates a new one if no buffers
  /// are cached.
  buffer_type next_header_buffer() {
    return next_buffer_impl(header_bufs_);
  }

  /// Returns the next cached payload_buffer or creates a new one if no buffers
  /// are cached.
  buffer_type next_payload_buffer() {
    return next_buffer_impl(payload_bufs_);
  }

private:
  // -- utility functions ------------------------------------------------------

  static buffer_type next_buffer_impl(buffer_cache_type cache) {
    if (cache.empty())
      return {};
    auto buf = std::move(cache.back());
    cache.pop_back();
    return buf;
  }

protected:
  next_layer_type next_layer_;
  handle_type handle_;

  buffer_cache_type header_bufs_;
  buffer_cache_type payload_bufs_;

  buffer_type read_buf_;

  endpoint_manager* manager_;
};

} // namespace caf::net
