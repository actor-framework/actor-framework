/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/hook.hpp"

#include "caf/message_id.hpp"

namespace caf {
namespace io {

hook::hook(actor_system& sys) : system_(sys) {
  // nop
}

hook::~hook() {
  // nop
}

void hook::message_received_cb(const node_id&, const strong_actor_ptr&,
                               const strong_actor_ptr&, message_id,
                               const message&) {
  // nop
}

void hook::message_sent_cb(const strong_actor_ptr&, const node_id&,
                           const strong_actor_ptr&, message_id,
                           const message&) {
  // nop
}

void hook::message_forwarded_cb(const basp::header&, const std::vector<char>*) {
  // nop
}

void hook::message_forwarding_failed_cb(const basp::header&,
                                        const std::vector<char>*) {
  // nop
}

void hook::message_sending_failed_cb(const strong_actor_ptr&,
                                     const strong_actor_ptr&,
                                     message_id, const message&) {
  // nop
}

void hook::actor_published_cb(const strong_actor_ptr&,
                              const std::set<std::string>&, uint16_t) {
  // nop
}

void hook::new_remote_actor_cb(const strong_actor_ptr&) {
  // nop
}

void hook::new_connection_established_cb(const node_id&) {
  // nop
}

void hook::new_route_added_cb(const node_id&, const node_id&) {
  // nop
}

void hook::connection_lost_cb(const node_id&) {
  // nop
}

void hook::route_lost_cb(const node_id&, const node_id&) {
  // nop
}

void hook::invalid_message_received_cb(const node_id&, const strong_actor_ptr&,
                                       actor_id, message_id, const message&) {
  // nop
}

void hook::before_shutdown_cb() {
  // nop
}

} // namespace io
} // namespace caf
