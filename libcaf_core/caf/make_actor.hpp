// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/adopt_ref.hpp"
#include "caf/detail/aligned_alloc.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/logger.hpp"
#include "caf/meta/handler.hpp"

#include <cstdlib>
#include <new>

namespace caf::detail {

// Has access to the required constructors (via friend declarations).
struct make_actor_util {
  template <class T, class... Args>
  static T* create_actor(void* mem, Args&&... args) {
    using traits = detail::control_block_traits<actor_control_block>;
    // Note: the constructor of abstract_actor sets the current actor to itself.
    //       Hence, we store the pointer to the current actor here and restore
    //       it after creating the new actor at scope exit.
    detail::scope_guard guard([prev = detail::current_actor()]() noexcept {
      detail::current_actor(prev);
    });
    auto* ptr = traits::construct_managed<T>(mem, std::forward<Args>(args)...);
    ptr->setup_metrics();
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
  using detail::make_actor_util;
  // Allocate enough memory for the control block and the actor object.
  using traits = detail::control_block_traits<actor_control_block>;
  auto* mem = traits::allocate<T>();
  auto* ctrl = traits::construct_ctrl(mem, aid, nid, sys, iface);
#ifdef CAF_ENABLE_TRACE_LOGGING
  if (auto* lptr = logger::current_logger();
      lptr && lptr->accepts(log::level::debug, CAF_LOG_FLOW_COMPONENT)) {
    auto args = deep_to_string(std::forward_as_tuple(xs...));
    auto* obj = make_actor_util::create_actor<T>(mem, std::forward<Ts>(xs)...);
#  ifdef CAF_ENABLE_RTTI
    lptr->log(log::level::debug, CAF_LOG_FLOW_COMPONENT,
              "SPAWN ; ID = {}; NAME = {}; TYPE = {}; ARGS = {}; NODE = {}",
              aid, obj->name(), detail::pretty_type_name(typeid(T).name()),
              args, nid);
#  else
    lptr->log(log::level::debug, CAF_LOG_FLOW_COMPONENT,
              "SPAWN ; ID = {}; NAME = {}; ARGS = {}; NODE = {}", aid,
              obj->name(), args, nid);
#  endif
    return {ctrl, adopt_ref};
  }
#endif
  detail::make_actor_util::create_actor<T>(mem, std::forward<Ts>(xs)...);
  return {ctrl, adopt_ref};
}

} // namespace caf
