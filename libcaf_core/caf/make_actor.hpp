// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_storage.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/logger.hpp"

namespace caf {

template <class T, class R = infer_handle_from_class_t<T>, class... Ts>
R make_actor(actor_id aid, node_id nid, actor_system* sys, Ts&&... xs) {
#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
  if (logger::current_logger()->accepts(CAF_LOG_LEVEL_DEBUG,
                                        CAF_LOG_FLOW_COMPONENT)) {
    std::string args;
    args = deep_to_string(std::forward_as_tuple(xs...));
    actor_storage<T>* ptr;
    {
      CAF_PUSH_AID(aid);
      ptr = new actor_storage<T>(aid, std::move(nid), sys,
                                 std::forward<Ts>(xs)...);
    }
    CAF_LOG_SPAWN_EVENT(ptr->data, args);
    ptr->data.setup_metrics();
    return {&(ptr->ctrl), false};
  }
#endif
  CAF_PUSH_AID(aid);
  auto ptr = new actor_storage<T>(aid, std::move(nid), sys,
                                  std::forward<Ts>(xs)...);
  ptr->data.setup_metrics();
  return {&(ptr->ctrl), false};
}

} // namespace caf
