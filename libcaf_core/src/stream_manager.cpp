/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#include "caf/sec.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/message.hpp"
#include "caf/expected.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/inbound_path.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_gatherer.hpp"
#include "caf/stream_scatterer.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {

stream_manager::~stream_manager() {
  // nop
}

error stream_manager::open(const stream_id& sid, strong_actor_ptr hdl,
                           strong_actor_ptr original_stage,
                           stream_priority prio, bool redeployable,
                           response_promise result_cb) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(original_stage)
                << CAF_ARG(prio) << CAF_ARG(redeployable));
  if (hdl == nullptr)
    return sec::invalid_argument;
  if (in().add_path(sid, hdl, std::move(original_stage), prio,
                    out().credit(), redeployable, std::move(result_cb))
      != nullptr)
    return none;
  return sec::cannot_add_upstream;
}

error stream_manager::ack_open(const stream_id& sid,
                               const actor_addr& rebind_from,
                               strong_actor_ptr rebind_to, long initial_demand,
                               bool redeployable) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(rebind_from) << CAF_ARG(rebind_to)
                << CAF_ARG(initial_demand) << CAF_ARG(redeployable));
  if (rebind_from == nullptr)
    return sec::invalid_argument;
  if (rebind_to == nullptr) {
    auto from_ptr = actor_cast<strong_actor_ptr>(rebind_from);
    if (from_ptr)
      out().remove_path(sid, from_ptr, sec::invalid_downstream, false);
    return sec::invalid_downstream;
  }
  auto ptr = out().confirm_path(sid, rebind_from, std::move(rebind_to),
                                initial_demand, redeployable);
  if (ptr == nullptr)
    return sec::invalid_downstream;
  downstream_demand(ptr, initial_demand);
  return none;
}

error stream_manager::batch(const stream_id& sid, const actor_addr& hdl,
                            long xs_size, message& xs, int64_t xs_id) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(xs_size)
                << CAF_ARG(xs) << CAF_ARG(xs_id));
  CAF_ASSERT(hdl != nullptr);
  auto ptr = in().find(sid, hdl);
  if (ptr == nullptr) {
    CAF_LOG_WARNING("received batch for unknown stream");
    return sec::invalid_downstream;
  }
  if (xs_size > ptr->assigned_credit) {
    CAF_LOG_WARNING("batch size of" << xs_size << "exceeds assigned credit of"
                    << ptr->assigned_credit);
    return sec::invalid_stream_state;
  }
  ptr->handle_batch(xs_size, xs_id);
  auto err = process_batch(xs);
  if (err == none) {
    push();
    auto current_size = out().buffered();
    auto desired_size = out().credit();
    if (current_size < desired_size)
      in().assign_credit(desired_size - current_size);
  }
  return err;
}

error stream_manager::ack_batch(const stream_id& sid, const actor_addr& hdl,
                                long demand, int64_t) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(demand));
  auto ptr = out().find(sid, hdl);
  if (ptr == nullptr)
    return sec::invalid_downstream;
  ptr->open_credit += demand;
  downstream_demand(ptr, demand);
  return none;
}

error stream_manager::close(const stream_id& sid, const actor_addr& hdl) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl));
  if (in().remove_path(sid, hdl, none, true) && in().closed())
    input_closed(none);
  return none;
}

error stream_manager::drop(const stream_id& sid, const actor_addr& hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  if (out().remove_path(sid, hdl, none, true) && out().closed())
    output_closed(none);
  return none;
}

error stream_manager::forced_close(const stream_id& sid, const actor_addr& hdl,
                                   error reason) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(reason));
  CAF_IGNORE_UNUSED(hdl);
  in().remove_path(sid, hdl, reason, true);
  abort(std::move(reason));
  return sec::unhandled_stream_error;
}

error stream_manager::forced_drop(const stream_id& sid, const actor_addr& hdl,
                                  error reason) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(reason));
  CAF_IGNORE_UNUSED(hdl);
  out().remove_path(sid, hdl, reason, true);
  abort(std::move(reason));
  return sec::unhandled_stream_error;
}

void stream_manager::abort(caf::error reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  in().abort(reason);
  input_closed(reason);
  out().abort(reason);
  output_closed(std::move(reason));
}

void stream_manager::close() {
  CAF_LOG_TRACE("");
  in().close(make_final_result());
  input_closed(none);
  out().close();
  output_closed(none);
}

bool stream_manager::add_sink(const stream_id& sid, strong_actor_ptr origin,
                        strong_actor_ptr sink_ptr,
                        mailbox_element::forwarding_stack stages,
                        message_id handshake_mid, message handshake_data,
                        stream_priority prio, bool redeployable) {
  return out().add_path(sid, std::move(origin), std::move(sink_ptr),
                        std::move(stages), handshake_mid,
                        std::move(handshake_data), prio, redeployable)
         != nullptr;
}

bool stream_manager::add_source(const stream_id& sid,
                                strong_actor_ptr source_ptr,
                                strong_actor_ptr original_stage,
                                stream_priority prio, bool redeployable,
                                response_promise result_cb) {
  // TODO: out().credit() gives the same amount of credit to any number of new
  //       sources -> feedback needed
  return in().add_path(sid, std::move(source_ptr), std::move(original_stage),
                       prio, out().credit(), redeployable, std::move(result_cb))
         != nullptr;
}

void stream_manager::push() {
  CAF_LOG_TRACE("");
  out().emit_batches();
}

bool stream_manager::generate_messages() {
  return false;
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
