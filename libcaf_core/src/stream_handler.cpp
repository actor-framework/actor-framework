/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/stream_handler.hpp"

#include "caf/sec.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/message.hpp"
#include "caf/expected.hpp"

namespace caf {

stream_handler::~stream_handler() {
  // nop
}

error stream_handler::add_downstream(strong_actor_ptr&, size_t, bool) {
  CAF_LOG_ERROR("Cannot add downstream to a stream marked as no-downstreams");
  return sec::cannot_add_downstream;
}

error stream_handler::downstream_ack(strong_actor_ptr&, int64_t) {
  CAF_LOG_ERROR("Received downstream messages in "
                "a stream marked as no-downstreams");
  return sec::invalid_downstream;
}

error stream_handler::downstream_demand(strong_actor_ptr&, size_t) {
  CAF_LOG_ERROR("Received downstream messages in "
                "a stream marked as no-downstreams");
  return sec::invalid_downstream;
}

error stream_handler::push() {
  CAF_LOG_ERROR("Cannot push to a stream marked as no-downstreams");
  return sec::invalid_downstream;
}

expected<size_t> stream_handler::add_upstream(strong_actor_ptr&,
                                              const stream_id&,
                                              stream_priority) {
  CAF_LOG_ERROR("Cannot add upstream to a stream marked as no-upstreams");
  return sec::cannot_add_upstream;
}

error stream_handler::upstream_batch(strong_actor_ptr&, size_t, message&) {
  CAF_LOG_ERROR("Received upstream messages in "
                "a stream marked as no-upstreams");
  return sec::invalid_upstream;
}

error stream_handler::close_upstream(strong_actor_ptr&) {
  CAF_LOG_ERROR("Received upstream messages in "
                "a stream marked as no-upstreams");
  return sec::invalid_upstream;
}

error stream_handler::pull(size_t) {
  CAF_LOG_ERROR("Cannot pull from a stream marked as no-upstreams");
  return sec::invalid_upstream;
}

bool stream_handler::is_sink() const {
  return false;
}

bool stream_handler::is_source() const {
  return false;
}

message stream_handler::make_output_token(const stream_id&) const {
  return make_message();
}

} // namespace caf
