// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_control_block.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/add_ref.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/panic.hpp"
#include "caf/error_code.hpp"
#include "caf/log/core.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/sec.hpp"

#include <cstdlib>

namespace caf {

actor_addr actor_control_block::address() noexcept {
  return {this, add_ref};
}

bool actor_control_block::enqueue(mailbox_element_ptr what, scheduler* sched) {
  return get()->enqueue(std::move(what), sched);
}

bool actor_control_block::delete_managed_object() noexcept {
  // When hitting 0, we need to allow the actor to clean up its state in case
  // it is not terminated yet. For this, we need to bump the ref count to 1
  // again, because the cleanup code might send messages to other actors that
  // in turn reference this actor.
  auto* ptr = get();
  if (!ptr->is_terminated()) {
#ifdef CAF_ENABLE_EXCEPTIONS
    try {
#endif
      // Make sure that other actors can no longer send messages to this actor.
      ptr->force_close_mailbox();
      // Bump the reference count and do the regular cleanup.
      ref_count_.inc_strong();
      ptr->on_unreachable();
#ifdef CAF_ENABLE_EXCEPTIONS
    } catch (const std::exception& ex) {
      detail::panic("failed to cleanup an unreachable actor: {}", ex.what());
    } catch (...) {
      detail::critical("failed to cleanup an unreachable actor: "
                       "unknown exception");
    }
#endif
    CAF_ASSERT(ptr->is_terminated());
    // Decrement the reference count again. If it reached 0 again, the caller
    // will release the implicit weak reference to the control block.
    if (ref_count_.dec_strong_unsafe()) {
      ptr->~abstract_actor();
      return true;
    }
    return false;
  }
  // The actor is already terminated. Hence, we can safely release the implicit
  // weak reference to the control block.
  ptr->~abstract_actor();
  return true;
}

error_code<sec> load_actor(strong_actor_ptr& ptr, actor_system* sys,
                           actor_id aid, const node_id& nid) {
  if (sys == nullptr)
    return error_code{sec::no_context};
  if (sys->node() == nid) {
    ptr = sys->registry().get(aid);
    log::core::debug("fetch actor handle from local actor registry: {}",
                     (ptr ? "found" : "not found"));
    return {};
  }
  // Get or create a proxy for the remote actor.
  if (auto* registry = proxy_registry::current()) {
    ptr = registry->get_or_put(nid, aid);
    return {};
  }
  return error_code{sec::no_proxy_registry};
}

error_code<sec> save_actor(const strong_actor_ptr& storage, actor_id aid,
                           const node_id& nid) {
  // Register locally running actors to be able to deserialize them later.
  if (storage) {
    auto& sys = storage->system();
    if (nid == sys.node())
      sys.registry().put(aid, storage);
  }
  return {};
}

namespace {

void append_to_string_impl(std::string& str, const actor_control_block* ptr) {
  if (ptr != nullptr) {
    auto& nid = ptr->node();
    if (wraps_uri(nid)) {
      append_to_string(str, nid);
      str += "/id/";
      str += std::to_string(ptr->id());
    } else {
      str += std::to_string(ptr->id());
      str += '@';
      append_to_string(str, nid);
    }
  } else {
    str += "null";
  }
}

std::string to_string_impl(const actor_control_block* ptr) {
  std::string result;
  append_to_string_impl(result, ptr);
  return result;
}

} // namespace

std::string to_string(const strong_actor_ptr& ptr) {
  return to_string_impl(ptr.get());
}

void append_to_string(std::string& str, const strong_actor_ptr& ptr) {
  return append_to_string_impl(str, ptr.get());
}

std::string to_string(const weak_actor_ptr& ptr) {
  return to_string_impl(ptr.get());
}

void append_to_string(std::string& str, const weak_actor_ptr& ptr) {
  return append_to_string_impl(str, ptr.get());
}

} // namespace caf
