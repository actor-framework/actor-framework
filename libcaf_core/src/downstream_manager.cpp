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

#include "caf/downstream_manager.hpp"

#include <functional>
#include <limits>

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

downstream_manager::downstream_manager::path_visitor::~path_visitor() {
  // nop
}

downstream_manager::downstream_manager::path_predicate::~path_predicate() {
  // nop
}

downstream_manager::downstream_manager(scheduled_actor* self) : self_(self) {
  // nop
}

downstream_manager::~downstream_manager() {
  // nop
}

// -- properties ---------------------------------------------------------------

bool downstream_manager::terminal() const noexcept {
  return true;
}

// -- path management ----------------------------------------------------------

size_t downstream_manager::num_paths() const noexcept {
  return 0;
}

downstream_manager::path_ptr
downstream_manager::add_path(stream_slot slot, strong_actor_ptr target) {
  CAF_LOG_TRACE(CAF_ARG(slot) << CAF_ARG(target));
  unique_path_ptr ptr{new outbound_path(slot, std::move(target))};
  auto result = ptr.get();
  return insert_path(std::move(ptr)) ? result : nullptr;
}

bool downstream_manager::remove_path(stream_slot, error, bool) noexcept {
  return false;
}

auto downstream_manager::path(stream_slot) noexcept -> path_ptr {
  return nullptr;
}

bool downstream_manager::clean() const noexcept {
  auto pred = [](const outbound_path& x) {
    return x.next_batch_id == x.next_ack_id;
  };
  return buffered() == 0 && all_paths(pred);
}

void downstream_manager::close() {
  CAF_LOG_TRACE("");
  for_each_path([&](outbound_path& x) { about_to_erase(&x, false, nullptr); });
  clear_paths();
}

void downstream_manager::abort(error reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  for_each_path([&](outbound_path& x) {
    auto tmp = reason;
    about_to_erase(&x, false, &tmp);
  });
  clear_paths();
}

size_t downstream_manager::min_credit() const {
  if (empty())
    return 0u;
  auto result = std::numeric_limits<size_t>::max();
  const_cast<downstream_manager*>(this)->for_each_path([&](outbound_path& x) {
    result = std::min(result, static_cast<size_t>(x.open_credit));
  });
  return result;
}

size_t downstream_manager::max_credit() const {
  size_t result = 0;
  const_cast<downstream_manager*>(this)->for_each_path([&](outbound_path& x) {
    result = std::max(result, static_cast<size_t>(x.open_credit));
  });
  return result;
}

size_t downstream_manager::total_credit() const {
  size_t result = 0;
  const_cast<downstream_manager*>(this)->for_each_path([&](outbound_path& x) {
    result += static_cast<size_t>(x.open_credit);
  });
  return result;
}

void downstream_manager::emit_batches() {
  // nop
}

void downstream_manager::force_emit_batches() {
  // nop
}

size_t downstream_manager::capacity() const noexcept {
  return std::numeric_limits<size_t>::max();
}

size_t downstream_manager::buffered() const noexcept {
  return 0;
}

void downstream_manager::clear_paths() {
  // nop
}

bool downstream_manager::insert_path(unique_path_ptr) {
  return false;
}

void downstream_manager::for_each_path_impl(path_visitor&) {
  // nop
}

bool downstream_manager::check_paths_impl(path_algorithm algo,
                                          path_predicate&) const noexcept {
  // Return the result for empty ranges as specified by the C++ standard.
  switch (algo) {
    default:
      CAF_ASSERT(algo == path_algorithm::all_of);
      return true;
    case path_algorithm::any_of:
      return false;
    case path_algorithm::none_of:
      return true;
  }
}

void downstream_manager::about_to_erase(outbound_path* ptr, bool silent,
                                      error* reason) {
  CAF_LOG_TRACE(CAF_ARG(ptr) << CAF_ARG(silent) << CAF_ARG(reason));
  if (!silent) {
    if (reason == nullptr)
      ptr->emit_regular_shutdown(self_);
    else
      ptr->emit_irregular_shutdown(self_, std::move(*reason));
  }
}

} // namespace caf
