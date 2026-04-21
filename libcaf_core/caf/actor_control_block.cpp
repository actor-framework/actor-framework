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

void actor_control_block::deref() noexcept {
  // Before hitting 0, we need to close the mailbox and to allow the actor to
  // clean up its state in case it is not terminated yet. For this, we need to
  // run the cleanup code while the strong reference count is still at 1,
  // because the cleanup code might send messages to other actors that in turn
  // will create new strong references to this actor. Hence, we can't simply
  // call `ref_count_.dec_strong(this)` here and instead fall back to a
  // compare-and-swap loop to ensure an actor is always terminated before its
  // strong reference count drops to 0.
  auto& strong = ref_count_.strong_reference_count_ref();
  auto count = strong.load(std::memory_order_acquire);
  for (size_t i = 0; i < 100; ++i) {
    if (count == 0) {
      detail::critical("tried to decrease the strong reference count "
                       "of an already expired object");
    }
    if (count > 1) {
      if (strong.compare_exchange_weak(count, count - 1,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
        return;
      }
      continue;
    }
    // The strong reference count would drop to 0 -> terminate the actor.
    auto* ptr = get();
    if (!ptr->is_terminated()) {
#ifdef CAF_ENABLE_EXCEPTIONS
      try {
#endif
        // Try to close the mailbox, i.e., transition from blocked to closed.
        if (!ptr->try_force_close_mailbox()) {
          // If closing the mailbox failed, another thread must have formed a
          // new strong reference to the actor and has put a new message into
          // the mailbox. Any actor with a non-empty mailbox must have at least
          // one strong reference from the scheduler. Retry.
          count = strong.load(std::memory_order_acquire);
          continue;
        }
        // Mailbox has been closed. Tell the actor to run any cleanup code and
        // to set its state to terminated.
        ptr->on_unreachable();
#ifdef CAF_ENABLE_EXCEPTIONS
      } catch (const std::exception& ex) {
        detail::panic("failed to cleanup an unreachable actor: {}", ex.what());
      } catch (...) {
        detail::critical("failed to cleanup an unreachable actor: "
                         "unknown exception");
      }
#endif
      // The actor must have been terminated after calling `on_unreachable`.
      CAF_ASSERT(ptr->is_terminated());
    }
    // Reaching here means the actor is terminated and we can safely call its
    // destructor. Run a new loop to decrement the reference count without
    // checking for `is_terminated` again.
    for (;;) {
      if (strong.compare_exchange_weak(count, count - 1,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
        if (count == 1) {
          ptr->~abstract_actor();
          deref_weak();
        }
        return;
      }
    }
  }
  detail::panic("failed to transition an expiring actor to terminated state "
                "after 100 attempts, id: {}",
                get()->id());
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
  detail::format_to(std::back_inserter(str), "{}",
                    detail::formatted{ptr, policy::by_reference});
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
  return to_string_impl(ptr.ctrl());
}

void append_to_string(std::string& str, const weak_actor_ptr& ptr) {
  return append_to_string_impl(str, ptr.ctrl());
}

} // namespace caf
