// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_config.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/adopt_ref.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/logger.hpp"
#include "caf/meta/handler.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace caf::detail {

template <class Actor>
class actor_control_block_impl final : public actor_control_block {
public:
  using super = actor_control_block;

  template <class... Args>
  actor_control_block_impl(actor_id aid, caf::node_id& nid, actor_system* sys,
                           const meta::handler_list* ifptr, actor_config& cfg,
                           Args&&... args)
    : super(aid, nid, sys, ifptr) {
    cfg.ctrl = this;
    // Note: the constructor of abstract_actor sets the current actor to itself.
    //       Hence, we store the pointer to the current actor here and restore
    //       it after creating the new actor at scope exit.
    scope_guard guard([prev = current_actor()]() noexcept { //
      current_actor(prev);
    });
    managed_ = &storage_;
    auto* ptr = new (&storage_) Actor(cfg, std::forward<Args>(args)...);
#ifdef CAF_ENABLE_EXCEPTIONS
    try {
      ptr->setup_metrics();
    } catch (...) {
      ptr->~Actor();
      throw;
    }
#else
    ptr->setup_metrics();
#endif
  }

  ~actor_control_block_impl() noexcept override {
    // nop
  }

private:
  void delete_this() noexcept override {
    delete this;
  }

  union {
    Actor storage_;
  };
};

} // namespace caf::detail

namespace caf {

template <class T, class R = infer_handle_from_class_t<T>, class... Args>
R make_actor(actor_id aid, node_id nid, actor_system* sys, actor_config& cfg,
             Args&&... args) {
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
  // Create the control block and the actor it manages.
  using block = detail::actor_control_block_impl<T>;
#ifdef CAF_ENABLE_TRACE_LOGGING
  if (auto* lptr = logger::current_logger();
      lptr && lptr->accepts(log::level::debug, CAF_LOG_FLOW_COMPONENT)) {
    auto args_str = deep_to_string(std::forward_as_tuple(args...));
    auto* ctrl = new block(aid, nid, sys, iface, cfg,
                           std::forward<Args>(args)...);
    auto res = R{ctrl, adopt_ref};
#  ifdef CAF_ENABLE_RTTI
    lptr->log(log::level::debug, CAF_LOG_FLOW_COMPONENT,
              "SPAWN ; ID = {}; NAME = {}; TYPE = {}; ARGS = {}; NODE = {}",
              aid, ctrl->managed()->name(),
              detail::pretty_type_name(typeid(T).name()), args_str, nid);
#  else
    lptr->log(log::level::debug, CAF_LOG_FLOW_COMPONENT,
              "SPAWN ; ID = {}; NAME = {}; ARGS = {}; NODE = {}", aid,
              ctrl->managed()->name(), args_str, nid);
#  endif
    return res;
  }
#endif
  return {new block(aid, nid, sys, iface, cfg, std::forward<Args>(args)...),
          adopt_ref};
}

} // namespace caf
