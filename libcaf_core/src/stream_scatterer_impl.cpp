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

#include "caf/stream_scatterer_impl.hpp"

#include <functional>

#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"

namespace caf {

stream_scatterer_impl::stream_scatterer_impl(local_actor* self) : super(self) {
  // nop
}

stream_scatterer_impl::~stream_scatterer_impl() {
  // nop
}

outbound_path* stream_scatterer_impl::add_path(stream_slots slots,
                                               strong_actor_ptr target) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(target));
  auto res = paths_.emplace(slots.sender, nullptr);
  if (res.second) {
    auto ptr = new outbound_path(slots, std::move(target));
    res.first->second.reset(ptr);
    return ptr;
  }
  return nullptr;
}

size_t stream_scatterer_impl::num_paths() const noexcept {
  return paths_.size();
}

auto stream_scatterer_impl::take_path(stream_slot slot) noexcept
-> unique_path_ptr {
  unique_path_ptr result;
  auto i = paths_.find(slot);
  if (i != paths_.end()) {
    about_to_erase(i->second.get(), true, nullptr);
    result.swap(i->second);
    paths_.erase(i);
  }
  return result;
}

auto stream_scatterer_impl::path(stream_slot slot) noexcept -> path_ptr {
  auto i = paths_.find(slot);
  return i != paths_.end() ? i->second.get() : nullptr;
}

void stream_scatterer_impl::clear_paths() {
  paths_.clear();
}

void stream_scatterer_impl::for_each_path_impl(path_visitor& f) {
  for (auto& kvp : paths_)
    f(*kvp.second);
}

bool stream_scatterer_impl::check_paths_impl(path_algorithm algo,
                                        path_predicate& pred) const noexcept {
  auto f = [&](const map_type::value_type& x) {
    return pred(*x.second);
  };
  switch (algo) {
    default: // all_of
      return std::all_of(paths_.begin(), paths_.end(), f);
    case path_algorithm::any_of:
      return std::any_of(paths_.begin(), paths_.end(), f);
    case path_algorithm::none_of:
      return std::none_of(paths_.begin(), paths_.end(), f);
  }
}

} // namespace caf
