// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/logger.hpp"
#include "caf/meta/handler.hpp"

#include <cstdlib>
#include <new>

namespace caf::detail {

// Has access to actor constructors (declared as friend).
struct make_actor_util {
  template <class T, class... Ts>
  static T* create_actor(void* mem, Ts&&... args) {
    auto* ptr = new (mem) T(std::forward<Ts>(args)...);
    ptr->setup_metrics();
    // Make sure that the pointer to the actor object is correct. Virtual
    // inheritance may mess with the memory layout, so we need to check that the
    // actor object actually starts at the right address.
    CAF_ASSERT(static_cast<abstract_actor*>(ptr) == mem);
    return ptr;
  }
};

} // namespace caf::detail

namespace caf {

template <class T, class R = infer_handle_from_class_t<T>, class... Ts>
R make_actor(actor_id aid, node_id nid, actor_system* sys, Ts&&... xs) {
  // Get the proper interface for the actor type.
  const meta::handler_list* iface;
  if constexpr (std::is_same_v<R, strong_actor_ptr>
                || std::is_same_v<R, actor>) {
    iface = nullptr;
  } else {
    using handlers_t
      = meta::handlers_from_signature_list<typename R::signatures>;
    iface = &handlers_t::handlers;
  }
  // Allocate enough memory for the control block and the actor object. The
  // control block is always padded to exactly one cache line so that the offset
  // of the actor object is always the same. This allows us to calculate the
  // address of the actor object from the address of the control block.
  static constexpr size_t alloc_size = CAF_CACHE_LINE_SIZE + sizeof(T);
  auto* mem = malloc(alloc_size);
  auto* ctrl = new (mem) actor_control_block(aid, nid, sys, iface);
  auto* obj_mem = reinterpret_cast<std::byte*>(mem) + CAF_CACHE_LINE_SIZE;
#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
  if (logger::current_logger()->accepts(CAF_LOG_LEVEL_DEBUG,
                                        CAF_LOG_FLOW_COMPONENT)) {
    std::string args;
    args = deep_to_string(std::forward_as_tuple(xs...));
    T* obj;
    {
      CAF_PUSH_AID(aid);
      obj = detail::make_actor_util::create_actor<T>(obj_mem,
                                                     std::forward<Ts>(xs)...);
    }
    CAF_LOG_SPAWN_EVENT(obj, args);
    return {ctrl, false};
  }
#endif
  CAF_PUSH_AID(aid);
  detail::make_actor_util::create_actor<T>(obj_mem, std::forward<Ts>(xs)...);
  return {ctrl, false};
}

} // namespace caf
