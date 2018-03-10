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

#include "caf/stream_scatterer.hpp"

#include <functional>

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"

namespace caf {

stream_scatterer::stream_scatterer::path_visitor::~path_visitor() {
  // nop
}

stream_scatterer::stream_scatterer::path_predicate::~path_predicate() {
  // nop
}

stream_scatterer::stream_scatterer(scheduled_actor* self) : self_(self) {
  // nop
}

stream_scatterer::~stream_scatterer() {
  // nop
}

bool stream_scatterer::clean() const noexcept {
  auto pred = [](const outbound_path& x) {
    return x.next_batch_id == x.next_ack_id;
  };
  return buffered() == 0 && all_paths(pred);
}

void stream_scatterer::close() {
  CAF_LOG_TRACE("");
  for_each_path([&](outbound_path& x) { about_to_erase(&x, false, nullptr); });
  clear_paths();
}

void stream_scatterer::abort(error reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  for_each_path([&](outbound_path& x) {
    auto tmp = reason;
    about_to_erase(&x, false, &tmp);
  });
  clear_paths();
}

size_t stream_scatterer::min_credit() const {
  if (empty())
    return 0u;
  auto result = std::numeric_limits<size_t>::max();
  const_cast<stream_scatterer*>(this)->for_each_path([&](outbound_path& x) {
    result = std::min(result, static_cast<size_t>(x.open_credit));
  });
  return result;
}

size_t stream_scatterer::max_credit() const {
  size_t result = 0;
  const_cast<stream_scatterer*>(this)->for_each_path([&](outbound_path& x) {
    result = std::max(result, static_cast<size_t>(x.open_credit));
  });
  return result;
}

size_t stream_scatterer::total_credit() const {
  size_t result = 0;
  const_cast<stream_scatterer*>(this)->for_each_path([&](outbound_path& x) {
    result += static_cast<size_t>(x.open_credit);
  });
  return result;
}

bool stream_scatterer::terminal() const noexcept {
  return false;
}

stream_scatterer::path_ptr stream_scatterer::add_path(stream_slot slot,
                                                      strong_actor_ptr target) {
  CAF_LOG_TRACE(CAF_ARG(slot) << CAF_ARG(target));
  unique_path_ptr ptr{new outbound_path(slot, std::move(target))};
  auto result = ptr.get();
  return insert_path(std::move(ptr)) ? result : nullptr;
}

void stream_scatterer::about_to_erase(outbound_path* ptr, bool silent,
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
