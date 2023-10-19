// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/actor_storage.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/thread_local_aid.hpp"

#include <type_traits>

namespace caf::detail {

void CAF_CORE_EXPORT log_spawn_event(monitorable_actor*);

} // namespace caf::detail

namespace caf {

template <class T, class R = infer_handle_from_class_t<T>, class... Ts>
R make_actor(actor_id aid, node_id nid, actor_system* sys, Ts&&... xs) {
  thread_local_aid_guard guard{aid};
  auto ptr = new actor_storage<T>(aid, std::move(nid), sys,
                                  std::forward<Ts>(xs)...);
  detail::log_spawn_event(&ptr->data);
  ptr->data.setup_metrics();
  return {&(ptr->ctrl), false};
}

} // namespace caf
