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

#include "caf/invalid_stream_scatterer.hpp"

#include "caf/logger.hpp"
#include "caf/message_builder.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream.hpp"
#include "caf/stream_slot.hpp"

namespace caf {

invalid_stream_scatterer::invalid_stream_scatterer(scheduled_actor* self)
    : stream_scatterer(self) {
  // nop
}

invalid_stream_scatterer::~invalid_stream_scatterer() {
  // nop
}

size_t invalid_stream_scatterer::num_paths() const noexcept {
  return 0;
}

bool invalid_stream_scatterer::remove_path(stream_slot, error, bool) noexcept {
  return false;
}

auto invalid_stream_scatterer::path(stream_slot) noexcept -> path_ptr {
  return nullptr;
}

void invalid_stream_scatterer::emit_batches() {
  // nop
}

void invalid_stream_scatterer::force_emit_batches() {
  // nop
}

size_t invalid_stream_scatterer::capacity() const noexcept {
  return 0u;
}

size_t invalid_stream_scatterer::buffered() const noexcept {
  return 0u;
}

bool invalid_stream_scatterer::insert_path(unique_path_ptr) {
  return false;
}

void invalid_stream_scatterer::for_each_path_impl(path_visitor&) {
  // nop
}

bool invalid_stream_scatterer::check_paths_impl(path_algorithm algo,
                                                path_predicate&)
                                               const noexcept {
  // Return the result for empty ranges as specified by the C++ standard.
  switch (algo) {
    default: // all_of
      return true;
    case path_algorithm::any_of:
      return false;
    case path_algorithm::none_of:
      return true;
  }
}

void invalid_stream_scatterer::clear_paths() {
  // nop
}

} // namespace caf
