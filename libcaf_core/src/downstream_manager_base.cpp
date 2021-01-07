// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/downstream_manager_base.hpp"

#include <functional>

#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/stream_manager.hpp"

namespace caf {

downstream_manager_base::downstream_manager_base(stream_manager* parent)
    : super(parent) {
  // nop
}

downstream_manager_base::downstream_manager_base(stream_manager* parent,
                                                 type_id_t type)
  : super(parent) {
  auto [pushed_elements, output_buffer_size]
    = parent->self()->outbound_stream_metrics(type);
  metrics_ = metrics_t{pushed_elements, output_buffer_size};
}

downstream_manager_base::~downstream_manager_base() {
  // nop
}

size_t downstream_manager_base::num_paths() const noexcept {
  return paths_.size();
}

bool downstream_manager_base::remove_path(stream_slot slot, error reason,
                                          bool silent) noexcept {
  CAF_LOG_TRACE(CAF_ARG(slot) << CAF_ARG(reason) << CAF_ARG(silent));
  auto i = paths_.find(slot);
  if (i != paths_.end()) {
    about_to_erase(i->second.get(), silent, reason ? &reason : nullptr);
    paths_.erase(i);
    return true;
  }
  return false;
}

auto downstream_manager_base::path(stream_slot slot) noexcept -> path_ptr {
  auto i = paths_.find(slot);
  return i != paths_.end() ? i->second.get() : nullptr;
}

void downstream_manager_base::clear_paths() {
  paths_.clear();
}

bool downstream_manager_base::insert_path(unique_path_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  CAF_ASSERT(ptr != nullptr);
  auto slot = ptr->slots.sender;
  CAF_ASSERT(slot != invalid_stream_slot);
  return paths_.emplace(slot, std::move(ptr)).second;
}

void downstream_manager_base::for_each_path_impl(path_visitor& f) {
  for (auto& kvp : paths_)
    f(*kvp.second);
}

bool downstream_manager_base::check_paths_impl(path_algorithm algo,
                                               path_predicate& pred)
                                               const noexcept {
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
