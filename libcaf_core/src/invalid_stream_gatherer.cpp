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

#include "caf/invalid_stream_gatherer.hpp"

#include "caf/logger.hpp"
#include "caf/response_promise.hpp"

namespace caf {

invalid_stream_gatherer::~invalid_stream_gatherer() {
  // nop
}

stream_gatherer::path_ptr
invalid_stream_gatherer::add_path(const stream_id&, strong_actor_ptr,
                                  strong_actor_ptr, stream_priority, long, bool,
                                  response_promise) {
  CAF_LOG_ERROR("invalid_stream_gatherer::add_path called");
  return nullptr;
}

bool invalid_stream_gatherer::remove_path(const stream_id&, const actor_addr&,
                                          error, bool) {
  CAF_LOG_ERROR("invalid_stream_gatherer::remove_path called");
  return false;
}

void invalid_stream_gatherer::close(message) {
  // nop
}

void invalid_stream_gatherer::abort(error) {
  // nop
}

long invalid_stream_gatherer::num_paths() const {
  return 0;
}

bool invalid_stream_gatherer::closed() const {
  return true;
}

bool invalid_stream_gatherer::continuous() const {
  return false;
}

void invalid_stream_gatherer::continuous(bool) {
  // nop
}

stream_gatherer::path_type* invalid_stream_gatherer::path_at(size_t) {
  return nullptr;
}

stream_gatherer::path_type* invalid_stream_gatherer::find(const stream_id&,
                                                          const actor_addr&) {
  return nullptr;
}

void invalid_stream_gatherer::assign_credit(long) {
  // nop
}

long invalid_stream_gatherer::initial_credit(long, path_type*) {
  return 0;
}

} // namespace caf
