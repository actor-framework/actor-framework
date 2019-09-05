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

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/byte.hpp"
#include "caf/error.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/header.hpp"
#include "caf/net/basp/message_type.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/node_id.hpp"
#include "caf/span.hpp"

namespace caf {
namespace net {
namespace basp {

class application {
public:
  // -- member types -----------------------------------------------------------

  // -- interface functions ----------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    system_ = &parent.system();
    return none;
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<endpoint_manager::message> ptr) {
    header hdr{message_type::actor_message,
               static_cast<uint32_t>(ptr->payload.size()),
               ptr->msg->mid.integer_value()};
    auto bytes = to_bytes(hdr);
    parent.write_packet(make_span(bytes), ptr->payload);
  }

  template <class Parent>
  error handle_data(Parent&, span<const byte> bytes) {
    return handle(bytes);
  }

  template <class Parent>
  void resolve(Parent&, const std::string&, actor) {
    // TODO: implement me
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

  connection_state state() const noexcept {
    return state_;
  }

  actor_system& system() const noexcept {
    return *system_;
  }

private:
  // -- message handling -------------------------------------------------------

  error handle(span<const byte> bytes);

  error handle(header hdr, span<const byte> payload);

  error handle_handshake(header hdr, span<const byte> payload);

  // -- member variables -------------------------------------------------------

  /// Stores a pointer to the parent actor system.
  actor_system* system_ = nullptr;

  /// Stores what we are expecting to receive next.
  connection_state state_ = connection_state::await_magic_number;

  /// Caches the last header;we need to store it when waiting for the payload.
  header hdr_;

  /// Stores our own ID.
  node_id id_;

  /// Stores the ID of our peer.
  node_id peer_id_;

  /// Keeps track of which local actors our peer monitors.
  std::unordered_set<actor_addr> monitored_actors_;
};

} // namespace basp
} // namespace net
} // namespace caf
