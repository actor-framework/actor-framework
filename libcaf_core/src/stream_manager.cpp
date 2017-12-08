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

#include "caf/stream_manager.hpp"

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/inbound_path.hpp"
#include "caf/logger.hpp"
#include "caf/message.hpp"
#include "caf/outbound_path.hpp"
#include "caf/response_promise.hpp"
#include "caf/sec.hpp"
#include "caf/stream_scatterer.hpp"

namespace caf {

stream_manager::stream_manager(local_actor* selfptr) : self_(selfptr) {
  // nop
}

stream_manager::~stream_manager() {
  // nop
}

error stream_manager::handle(inbound_path* from, downstream_msg::batch& x) {
  return none;
}

error stream_manager::handle(inbound_path* from, downstream_msg::close& x) {
  out().take_path(from->slots);
  return none;
}

error stream_manager::handle(inbound_path* from,
                             downstream_msg::forced_close& x) {
  return none;
}

error stream_manager::handle(outbound_path* from, upstream_msg::ack_open& x) {
  return none;
}

error stream_manager::handle(outbound_path* from, upstream_msg::ack_batch& x) {
  return none;
}

error stream_manager::handle(outbound_path* from, upstream_msg::drop& x) {
  return none;
}

error stream_manager::handle(outbound_path* from,
                             upstream_msg::forced_drop& x) {
  return none;
}

void stream_manager::close() {
  out().close();
  // TODO: abort all input pahts as well
}

void stream_manager::abort(error reason) {
  out().abort(std::move(reason));
  // TODO: abort all input pahts as well
}

void stream_manager::push() {
  CAF_LOG_TRACE("");
  do {
    out().emit_batches();
  } while (generate_messages());
}

bool stream_manager::congested() const {
  return false;
}

bool stream_manager::generate_messages() {
  return false;
}

void stream_manager::register_input_path(inbound_path* ptr) {
  inbound_paths_.emplace_back(ptr);
}

void stream_manager::deregister_input_path(inbound_path* ptr) noexcept {
  using std::swap;
  if (ptr != inbound_paths_.back()) {
    auto i = std::find(inbound_paths_.begin(), inbound_paths_.end(), ptr);
    CAF_ASSERT(i != inbound_paths_.end());
    swap(*i, inbound_paths_.back());
  }
  inbound_paths_.pop_back();
}

message stream_manager::make_final_result() {
  return none;
}

error stream_manager::process_batch(message&) {
  CAF_LOG_ERROR("stream_manager::process_batch called");
  return sec::invalid_stream_state;
}

void stream_manager::output_closed(error) {
  // nop
}

message stream_manager::make_output_token(const stream_id&) const {
  CAF_LOG_ERROR("stream_manager::make_output_token called");
  return none;
}

void stream_manager::downstream_demand(outbound_path*, long) {
  CAF_LOG_ERROR("stream_manager::downstream_demand called");
}

void stream_manager::input_closed(error) {
  // nop
}

} // namespace caf
