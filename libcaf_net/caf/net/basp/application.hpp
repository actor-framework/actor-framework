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

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/byte.hpp"
#include "caf/callback.hpp"
#include "caf/error.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/header.hpp"
#include "caf/net/basp/message_type.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/node_id.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/response_promise.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/span.hpp"
#include "caf/unit.hpp"

namespace caf {
namespace net {
namespace basp {

class application {
public:
  // -- member types -----------------------------------------------------------

  using buffer_type = std::vector<byte>;

  using byte_span = span<const byte>;

  using write_packet_callback = callback<byte_span, byte_span>;

  using proxy_registry_ptr = std::shared_ptr<proxy_registry>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit application(proxy_registry_ptr proxies);

  // -- interface functions ----------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    // Initialize member variables.
    system_ = &parent.system();
    // Write handshake.
    if (auto err = generate_handshake())
      return err;
    auto hdr = to_bytes(header{message_type::handshake,
                               static_cast<uint32_t>(buf_.size()), version});
    parent.write_packet(hdr, buf_);
    parent.transport().configure_read(receive_policy::exactly(header_size));
    return none;
  }

  template <class Parent>
  error write_message(Parent& parent,
                      std::unique_ptr<endpoint_manager::message> ptr) {
    // TODO: avoid extra copy of the payload
    buf_.clear();
    serializer_impl<buffer_type> sink{system(), buf_};
    const auto& src = ptr->msg->sender;
    const auto& dst = ptr->receiver;
    if (dst == nullptr) {
      // TODO: valid?
      return none;
    }
    if (src != nullptr) {
      if (auto err = sink(src->node(), src->id(), dst->id(), ptr->msg->stages))
        return err;
    } else {
      if (auto err = sink(node_id{}, actor_id{0}, dst->id(), ptr->msg->stages))
        return err;
    }
    buf_.insert(buf_.end(), ptr->payload.begin(), ptr->payload.end());
    header hdr{message_type::actor_message, static_cast<uint32_t>(buf_.size()),
               ptr->msg->mid.integer_value()};
    auto bytes = to_bytes(hdr);
    parent.write_packet(make_span(bytes), make_span(buf_));
    return none;
  }

  template <class Parent>
  error handle_data(Parent& parent, byte_span bytes) {
    auto write_packet = make_callback([&](byte_span hdr, byte_span payload) {
      parent.write_packet(hdr, payload);
      return none;
    });
    size_t next_read_size = header_size;
    if (auto err = handle(next_read_size, write_packet, bytes))
      return err;
    parent.transport().configure_read(receive_policy::exactly(next_read_size));
    return none;
  }

  template <class Parent>
  void resolve(Parent& parent, string_view path, actor listener) {
    auto write_packet = make_callback([&](byte_span hdr, byte_span payload) {
      parent.write_packet(hdr, payload);
      return none;
    });
    resolve_remote_path(write_packet, path, listener);
  }

  template <class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }

  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const type_erased_tuple& x);

  // -- utility functions ------------------------------------------------------

  strong_actor_ptr resolve_local_path(string_view path);

  void resolve_remote_path(write_packet_callback& write_packet,
                           string_view path, actor listener);

  // -- properties -------------------------------------------------------------

  connection_state state() const noexcept {
    return state_;
  }

  actor_system& system() const noexcept {
    return *system_;
  }

private:
  // -- message handling -------------------------------------------------------

  error handle(size_t& next_read_size, write_packet_callback& write_packet,
               byte_span bytes);

  error handle(write_packet_callback& write_packet, header hdr,
               byte_span payload);

  error handle_handshake(write_packet_callback& write_packet, header hdr,
                         byte_span payload);

  error handle_actor_message(write_packet_callback& write_packet, header hdr,
                             byte_span payload);

  error handle_resolve_request(write_packet_callback& write_packet, header hdr,
                               byte_span payload);

  error handle_resolve_response(write_packet_callback& write_packet, header hdr,
                                byte_span payload);

  /// Writes the handshake payload to `buf_`.
  error generate_handshake();

  // -- member variables -------------------------------------------------------

  /// Stores a pointer to the parent actor system.
  actor_system* system_ = nullptr;

  /// Stores what we are expecting to receive next.
  connection_state state_ = connection_state::await_handshake_header;

  /// Caches the last header;we need to store it when waiting for the payload.
  header hdr_;

  /// Re-usable buffer for storing payloads.
  buffer_type buf_;

  /// Stores our own ID.
  node_id id_;

  /// Stores the ID of our peer.
  node_id peer_id_;

  /// Keeps track of which local actors our peer monitors.
  std::unordered_set<actor_addr> monitored_actors_;

  /// Caches actor handles obtained via `resolve`.
  std::unordered_map<uint64_t, response_promise> pending_resolves_;

  /// Ascending ID generator for requests to our peer.
  uint64_t next_request_id_ = 1;

  /// Points to the factory object for generating proxies.
  proxy_registry_ptr proxies_;
};

} // namespace basp
} // namespace net
} // namespace caf
