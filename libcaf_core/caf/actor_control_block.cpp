// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_control_block.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/sec.hpp"

namespace caf {

actor_addr actor_control_block::address() {
  return {this, true};
}

bool actor_control_block::enqueue(mailbox_element_ptr what,
                                  execution_unit* host) {
  return get()->enqueue(std::move(what), host);
}

bool intrusive_ptr_upgrade_weak(actor_control_block* x) {
  auto count = x->strong_refs.load();
  while (count != 0)
    if (x->strong_refs.compare_exchange_weak(count, count + 1,
                                             std::memory_order_relaxed))
      return true;
  return false;
}

void intrusive_ptr_release_weak(actor_control_block* x) {
  // destroy object if last weak pointer expires
  if (x->weak_refs == 1
      || x->weak_refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    x->block_dtor(x);
}

void intrusive_ptr_release(actor_control_block* x) {
  if (x->strong_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    // When hitting 0, we need to allow the actor to clean up its state in case
    // it is not terminated yet. For this, we need to bump the ref count to 1
    // again, because the cleanup code might send messages to other actors that
    // in turn reference this actor.
    auto* ptr = x->get();
    if (!ptr->is_terminated()) {
      x->strong_refs.fetch_add(1, std::memory_order_relaxed);
      ptr->on_unreachable();
      CAF_ASSERT(ptr->is_terminated());
      if (x->strong_refs.fetch_sub(1, std::memory_order_acq_rel) != 1) {
        // Another strong reference was added while we were cleaning up.
        return;
      }
    }
    x->data_dtor(x->get());
    // We release the implicit weak pointer if the last strong ref expires and
    // destroy the data block.
    intrusive_ptr_release_weak(x);
  }
}

bool operator==(const strong_actor_ptr& x, const abstract_actor* y) {
  return x.get() == actor_control_block::from(y);
}

bool operator==(const abstract_actor* x, const strong_actor_ptr& y) {
  return actor_control_block::from(x) == y.get();
}

error_code<sec> load_actor(strong_actor_ptr& storage, execution_unit* ctx,
                           actor_id aid, const node_id& nid) {
  if (ctx == nullptr)
    return sec::no_context;
  auto& sys = ctx->system();
  if (sys.node() == nid) {
    storage = sys.registry().get(aid);
    CAF_LOG_DEBUG("fetch actor handle from local actor registry: "
                  << (storage ? "found" : "not found"));
    return none;
  }
  auto prp = ctx->proxy_registry_ptr();
  if (prp == nullptr)
    return sec::no_proxy_registry;
  // deal with (proxies for) remote actors
  storage = prp->get_or_put(nid, aid);
  return none;
}

error_code<sec> save_actor(strong_actor_ptr& storage, execution_unit* ctx,
                           actor_id aid, const node_id& nid) {
  if (ctx == nullptr)
    return sec::no_context;
  auto& sys = ctx->system();
  // register locally running actors to be able to deserialize them later
  if (nid == sys.node())
    sys.registry().put(aid, storage);
  return none;
}

namespace {

void append_to_string_impl(std::string& x, const actor_control_block* y) {
  if (y != nullptr) {
    if (wraps_uri(y->nid)) {
      append_to_string(x, y->nid);
      x += "/id/";
      x += std::to_string(y->aid);
    } else {
      x += std::to_string(y->aid);
      x += '@';
      append_to_string(x, y->nid);
    }
  } else {
    x += "null";
  }
}

std::string to_string_impl(const actor_control_block* x) {
  std::string result;
  append_to_string_impl(result, x);
  return result;
}

} // namespace

std::string to_string(const strong_actor_ptr& x) {
  return to_string_impl(x.get());
}

void append_to_string(std::string& x, const strong_actor_ptr& y) {
  return append_to_string_impl(x, y.get());
}

std::string to_string(const weak_actor_ptr& x) {
  return to_string_impl(x.get());
}

void append_to_string(std::string& x, const weak_actor_ptr& y) {
  return append_to_string_impl(x, y.get());
}

} // namespace caf
