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
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte.hpp"
#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/worker_hub.hpp"
#include "caf/error.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/header.hpp"
#include "caf/net/basp/message_queue.hpp"
#include "caf/net/basp/message_type.hpp"
#include "caf/net/basp/worker.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/packet_writer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/node_id.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/response_promise.hpp"
#include "caf/scoped_execution_unit.hpp"
#include "caf/unit.hpp"

namespace caf::net::basp {

/// An implementation of BASP as an application layer protocol.
class application {
public:
  // -- member types -----------------------------------------------------------

  using byte_span = span<const byte>;

  using hub_type = detail::worker_hub<worker>;

  struct test_tag {};

  // -- constructors, destructors, and assignment operators --------------------

  explicit application(proxy_registry& proxies);

  // -- static utility functions -----------------------------------------------

  static auto default_app_ids() {
    return std::vector<std::string>{
      to_string(defaults::middleman::app_identifier)};
  }

  // -- interface functions ----------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    // Initialize member variables.
    system_ = &parent.system();
    executor_.system_ptr(system_);
    executor_.proxy_registry_ptr(&proxies_);
    // TODO: use `if constexpr` when switching to C++17.
    // Allow unit tests to run the application without endpoint manager.
    if (!std::is_base_of<test_tag, Parent>::value)
      manager_ = &parent.manager();
    size_t workers;
    if (auto workers_cfg = get_if<size_t>(&system_->config(),
                                          "middleman.workers"))
      workers = *workers_cfg;
    else
      workers = std::min(3u, std::thread::hardware_concurrency() / 4u) + 1;
    for (size_t i = 0; i < workers; ++i)
      hub_->add_new_worker(*queue_, proxies_);
    // Write handshake.
    auto hdr = parent.next_header_buffer();
    auto payload = parent.next_payload_buffer();
    if (auto err = generate_handshake(payload))
      return err;
    to_bytes(header{message_type::handshake,
                    static_cast<uint32_t>(payload.size()), version},
             hdr);
    parent.write_packet(hdr, payload);
    parent.transport().configure_read(receive_policy::exactly(header_size));
    return none;
  }

  error write_message(packet_writer& writer,
                      std::unique_ptr<endpoint_manager_queue::message> ptr);

  template <class Parent>
  error handle_data(Parent& parent, byte_span bytes) {
    static_assert(std::is_base_of<packet_writer, Parent>::value,
                  "parent must implement packet_writer");
    size_t next_read_size = header_size;
    if (auto err = handle(next_read_size, parent, bytes))
      return err;
    parent.transport().configure_read(receive_policy::exactly(next_read_size));
    return none;
  }

  void resolve(packet_writer& writer, string_view path, const actor& listener);

  static void new_proxy(packet_writer& writer, actor_id id);

  void local_actor_down(packet_writer& writer, actor_id id, error reason);

  template <class Parent>
  void timeout(Parent&, const std::string&, uint64_t) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }

  // -- utility functions ------------------------------------------------------

  strong_actor_ptr resolve_local_path(string_view path);

  // -- properties -------------------------------------------------------------

  connection_state state() const noexcept {
    return state_;
  }

  actor_system& system() const noexcept {
    return *system_;
  }

private:
  // -- handling of incoming messages ------------------------------------------

  error handle(size_t& next_read_size, packet_writer& writer, byte_span bytes);

  error handle(packet_writer& writer, header hdr, byte_span payload);

  error handle_handshake(packet_writer& writer, header hdr, byte_span payload);

  error handle_actor_message(packet_writer& writer, header hdr,
                             byte_span payload);

  error handle_resolve_request(packet_writer& writer, header rec_hdr,
                               byte_span received);

  error handle_resolve_response(packet_writer& writer, header received_hdr,
                                byte_span received);

  error handle_monitor_message(packet_writer& writer, header received_hdr,
                               byte_span received);

  error handle_down_message(packet_writer& writer, header received_hdr,
                            byte_span received);

  /// Writes the handshake payload to `buf_`.
  error generate_handshake(byte_buffer& buf);

  // -- member variables -------------------------------------------------------

  /// Stores a pointer to the parent actor system.
  actor_system* system_ = nullptr;

  /// Stores the expected type of the next incoming message.
  connection_state state_ = connection_state::await_handshake_header;

  /// Caches the last header while waiting for the matching payload.
  header hdr_;

  /// Stores the ID of our peer.
  node_id peer_id_;

  /// Tracks which local actors our peer monitors.
  std::unordered_set<actor_addr> monitored_actors_; // TODO: this is unused

  /// Caches actor handles obtained via `resolve`.
  std::unordered_map<uint64_t, actor> pending_resolves_;

  /// Ascending ID generator for requests to our peer.
  uint64_t next_request_id_ = 1;

  /// Points to the factory object for generating proxies.
  proxy_registry& proxies_;

  /// Points to the endpoint manager that owns this applications.
  endpoint_manager* manager_ = nullptr;

  /// Provides pointers to the actor system as well as the registry,
  /// serializers and deserializer.
  scoped_execution_unit executor_;

  std::unique_ptr<message_queue> queue_;

  std::unique_ptr<hub_type> hub_;
};

} // namespace caf::net::basp
