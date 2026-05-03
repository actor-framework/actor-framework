// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/abstract_actor.hpp"

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/add_ref.hpp"
#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/internal/attachable_factory.hpp"
#include "caf/internal/attachable_predicate.hpp"
#include "caf/log/core.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include <atomic>
#include <mutex>

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

abstract_actor::abstract_actor(actor_config& cfg) : flags_(cfg.flags) {
  detail::current_actor(this);
}

abstract_actor::~abstract_actor() noexcept {
  // nop
}

// -- attachables ------------------------------------------------------------

bool abstract_actor::attach(attachable_ptr ptr) {
  auto lg = log::core::trace("");
  CAF_ASSERT(ptr != nullptr);
  error fail_state;
  auto attached = exclusive_critical_section([this, &fail_state, &ptr] {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      return false;
    }
    attachables_.push(std::move(ptr));
    return true;
  });
  if (!attached) {
    log::core::debug("cannot attach to terminated actor: call immediately");
    ptr->actor_exited(this, fail_state, nullptr);
    return false;
  }
  return true;
}

// Note: add_monitor / del_monitor keep incoming_edges_ in sync: one inc after a
// successful attach on the observed actor, one dec after erase_first_if removes
// a monitor attachable there. Only cleanup() does not call dec, but it will
// clear the entire map anyway. The third function to modify incoming_edges_ is
// clear_incoming_edges, which is called after receiving a `down_msg` from
// another actor. Since the other actor has terminated once we receive its
// `down_msg`, any subsequent `del_monitor` call will not find any matching
// entry in the map and thus will not attempt to decrement the count (which
// would result in a panic).

void abstract_actor::add_monitor(abstract_actor* observed,
                                 attachable_ptr&& what) {
  CAF_ASSERT(observed != nullptr);
  CAF_ASSERT(what != nullptr);
  if (observed->attach(std::move(what))) {
    // Increment how many monitor attachables we have added to `observed`.
    // During cleanup, we need to remove those attachables from `observed`
    // again to prevent stale references.
    auto* observed_ctrl = observed->ctrl();
    exclusive_critical_section([this, observed_ctrl] { //
      if (auto iter = incoming_edges_.find(observed_ctrl);
          iter != incoming_edges_.end()) {
        ++iter->second;
        return;
      }
      auto& entries = incoming_edges_.container();
      entries.emplace_back(weak_actor_ptr{observed_ctrl, add_ref}, 1);
    });
  }
}

void abstract_actor::del_monitor(abstract_actor* observed,
                                 const internal::attachable_predicate& pred) {
  CAF_ASSERT(observed != nullptr);
  auto removed = observed->exclusive_critical_section([observed, &pred] { //
    return observed->attachables_.erase_first_if(pred);
  });
  if (removed) {
    // Decrement how many monitor attachables we have added to `observed`. If
    // this was the last one, we can remove the entry from `incoming_edges_` and
    // skip the extra steps during cleanup.
    auto* observed_ctrl = observed->ctrl();
    exclusive_critical_section([this, observed_ctrl] { //
      auto iter = incoming_edges_.find(observed_ctrl);
      if (iter != incoming_edges_.end()) {
        if (--iter->second == 0) {
          incoming_edges_.erase(iter);
        }
        return;
      }
      log::core::error("cannot decrement incoming edges to unknown actor {}",
                       observed_ctrl);
    });
  }
}

// -- linking ------------------------------------------------------------------

void abstract_actor::unlink_from(const actor_addr& other) {
  auto lg = log::core::trace("other = {}", other);
  if (other.id() == invalid_actor_id)
    return;
  // Try to resolve `other` to a weak pointer.
  weak_actor_ptr other_ptr;
  exclusive_critical_section([this, &other, &other_ptr] { //
    internal::attachable_predicate::extractor extractor{&other, &other_ptr};
    auto pred = internal::attachable_predicate::linked_to(&extractor);
    std::ignore = this->attachables_.any_of(pred);
  });
  // If there's no link attachable for `other`, we're done.
  if (!other_ptr) {
    return;
  }
  // If `other` is still alive, unlink by calling remove_link.
  if (auto hdl = other_ptr.lock()) {
    CAF_ASSERT(hdl.get() != ctrl());
    remove_link(hdl->get());
    return;
  }
  // Promoting weak to strong reference fails if-and-only-if the strong
  // reference count is already at 0 which means the other actor must have
  // terminated already. Clean up leftover state.
  exclusive_critical_section([this, &other] { //
    internal::attachable_predicate::extractor extractor{&other, nullptr};
    auto pred = internal::attachable_predicate::linked_to(&extractor);
    attachables_.erase_first_if(pred);
  });
}

// -- properties ---------------------------------------------------------------

std::set<std::string> abstract_actor::message_types() const {
  // defaults to untyped
  return std::set<std::string>{};
}

actor_id abstract_actor::id() const noexcept {
  return actor_control_block::from(this)->id();
}

node_id abstract_actor::node() const noexcept {
  return actor_control_block::from(this)->node();
}

actor_system& abstract_actor::home_system() const noexcept {
  return actor_control_block::from(this)->system();
}

actor_control_block* abstract_actor::ctrl() const {
  return actor_control_block::from(this);
}

actor_addr abstract_actor::address() const noexcept {
  return {id(), node()};
}

abstract_actor* abstract_actor::current() noexcept {
  return detail::current_actor();
}

// -- callbacks ----------------------------------------------------------------

void abstract_actor::on_unreachable() {
  detail::current_actor_guard ctx_guard{this};
  cleanup(make_error(exit_reason::unreachable), nullptr);
}

void abstract_actor::on_cleanup(const error&) {
  // nop
}

bool abstract_actor::cleanup(error&& reason, scheduler* sched) {
  auto lg = log::core::trace("reason = {}", reason);
  decltype(attachables_) attachables;
  decltype(incoming_edges_) incoming;
  auto fs = 0;
  bool do_cleanup = exclusive_critical_section([&, this]() -> bool {
    fs = flags();
    if ((fs & is_terminated_flag) == 0) {
      // local actors pass fail_state_ as first argument
      if (&fail_state_ != &reason)
        fail_state_ = std::move(reason);
      attachables_.swap(attachables);
      incoming_edges_.swap(incoming);
      flags(fs | is_terminated_flag);
      return true;
    }
    return false;
  });
  if (!do_cleanup)
    return false;
  log::core::debug("actor {} cleans up with reason {}", id(), fail_state_);
  // Phase 1: clean up attachables from other actors that point to this actor.
  auto pred = internal::attachable_predicate::observed_by(ctrl());
  for (auto& kvp : incoming) {
    if (auto peer = kvp.first.lock()) {
      auto* ptr = actor_cast<abstract_actor*>(peer.get());
      ptr->exclusive_critical_section([ptr, &pred] { //
        ptr->attachables_.erase_if(pred);
      });
    }
  }
  // Phase 2: trigger actor_exited for all attachables.
  attachables.for_each([this, sched](attachable& entry) {
    entry.actor_exited(this, fail_state_, sched);
  });
  if (getf(is_registered_flag)) {
    home_system().dec_running_actors_count(id());
  }
  on_cleanup(fail_state_);
  return true;
}

void abstract_actor::ref() const noexcept {
  ctrl()->ref();
}

void abstract_actor::deref() const noexcept {
  ctrl()->deref();
}

void abstract_actor::add_link(abstract_actor* x) {
  // Add backlink on `x` first and add the local attachable only on success.
  // Note: links are bidirectional, so they don't produce incoming edges.
  auto lg = log::core::trace("x = {}", x);
  CAF_ASSERT(x != nullptr);
  error fail_state;
  bool send_exit_immediately = false;
  auto tmp = internal::attachable_factory::make_link(
    weak_actor_ptr{x->ctrl(), add_ref});
  joined_exclusive_critical_section(this, x, [&] {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      send_exit_immediately = true;
    } else if (x->add_backlink(this)) {
      attachables_.push(std::move(tmp));
    }
  });
  if (send_exit_immediately) {
    auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                    exit_msg{address(), fail_state});
    x->enqueue(std::move(ptr), nullptr);
  }
}

void abstract_actor::remove_link(abstract_actor* x) {
  auto lg = log::core::trace("x = {}", x);
  auto pred = internal::attachable_predicate::linked_to(x->ctrl());
  joined_exclusive_critical_section(this, x, [&] {
    x->remove_backlink(this);
    attachables_.erase_first_if(pred);
  });
}

bool abstract_actor::add_backlink(abstract_actor* x) {
  // Called in an exclusive critical section, i.e., while holding `mtx_`.
  auto lg = log::core::trace("x = {}", x);
  CAF_ASSERT(x);
  if (getf(is_terminated_flag)) {
    auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                    exit_msg{address(), fail_state_});
    x->enqueue(std::move(ptr), nullptr);
    return false;
  }
  auto pred = internal::attachable_predicate::linked_to(x->ctrl());
  if (!attachables_.any_of(pred)) {
    auto tmp = internal::attachable_factory::make_link(
      weak_actor_ptr{x->ctrl(), add_ref});
    attachables_.push(std::move(tmp));
    return true;
  }
  return false;
}

bool abstract_actor::remove_backlink(abstract_actor* x) {
  // Called in an exclusive critical section.
  auto lg = log::core::trace("x = {}", x);
  auto pred = internal::attachable_predicate::linked_to(x->ctrl());
  return attachables_.erase_first_if(pred);
}

void abstract_actor::clear_incoming_edges(const actor_addr& other) {
  auto lg = log::core::trace("other = {}", other);
  exclusive_critical_section([this, &other] { //
    auto iter = incoming_edges_.find(other);
    if (iter != incoming_edges_.end()) {
      incoming_edges_.erase(iter);
    }
  });
}

} // namespace caf
