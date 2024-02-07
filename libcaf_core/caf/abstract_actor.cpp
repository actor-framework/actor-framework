// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/abstract_actor.hpp"

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/default_attachable.hpp"
#include "caf/execution_unit.hpp"
#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include <atomic>
#include <mutex>

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

abstract_actor::abstract_actor(actor_config& cfg) : flags_(cfg.flags) {
  // nop
}

abstract_actor::~abstract_actor() {
  // nop
}

// -- attachables ------------------------------------------------------------

void abstract_actor::attach(attachable_ptr ptr) {
  CAF_LOG_TRACE("");
  CAF_ASSERT(ptr != nullptr);
  error fail_state;
  auto attached = exclusive_critical_section([&] {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      return false;
    }
    attach_impl(ptr);
    return true;
  });
  if (!attached) {
    CAF_LOG_DEBUG(
      "cannot attach functor to terminated actor: call immediately");
    ptr->actor_exited(fail_state, nullptr);
  }
}

size_t abstract_actor::detach(const attachable::token& what) {
  CAF_LOG_TRACE("");
  std::unique_lock<std::mutex> guard{mtx_};
  return detach_impl(what);
}

void abstract_actor::attach_impl(attachable_ptr& ptr) {
  ptr->next.swap(attachables_head_);
  attachables_head_.swap(ptr);
}

size_t abstract_actor::detach_impl(const attachable::token& what,
                                   bool stop_on_hit, bool dry_run) {
  CAF_LOG_TRACE(CAF_ARG(stop_on_hit) << CAF_ARG(dry_run));
  size_t count = 0;
  auto i = &attachables_head_;
  while (*i != nullptr) {
    if ((*i)->matches(what)) {
      ++count;
      if (!dry_run) {
        CAF_LOG_DEBUG("removed element");
        attachable_ptr next;
        next.swap((*i)->next);
        (*i).swap(next);
      } else {
        i = &((*i)->next);
      }
      if (stop_on_hit)
        return count;
    } else {
      i = &((*i)->next);
    }
  }
  return count;
}

// -- linking ------------------------------------------------------------------

void abstract_actor::link_to(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(other));
  link_to(actor_cast<strong_actor_ptr>(other));
}

void abstract_actor::unlink_from(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(other));
  if (!other)
    return;
  if (auto hdl = actor_cast<strong_actor_ptr>(other)) {
    unlink_from(hdl);
    return;
  }
  default_attachable::observe_token tk{other, default_attachable::link};
  exclusive_critical_section([&] { detach_impl(tk, true); });
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
  return *(actor_control_block::from(this)->home_system);
}

actor_control_block* abstract_actor::ctrl() const {
  return actor_control_block::from(this);
}

actor_addr abstract_actor::address() const noexcept {
  return actor_addr{actor_control_block::from(this)};
}

// -- callbacks ----------------------------------------------------------------

void abstract_actor::on_unreachable() {
  CAF_PUSH_AID_FROM_PTR(this);
  cleanup(make_error(exit_reason::unreachable), nullptr);
}

void abstract_actor::on_cleanup(const error&) {
  // nop
}

bool abstract_actor::cleanup(error&& reason, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  attachable_ptr head;
  auto fs = 0;
  bool do_cleanup = exclusive_critical_section([&, this]() -> bool {
    fs = flags();
    if ((fs & is_terminated_flag) == 0) {
      // local actors pass fail_state_ as first argument
      if (&fail_state_ != &reason)
        fail_state_ = std::move(reason);
      attachables_head_.swap(head);
      flags(fs | is_terminated_flag);
      return true;
    }
    return false;
  });
  if (!do_cleanup)
    return false;
  CAF_LOG_DEBUG("cleanup" << CAF_ARG(id()) << CAF_ARG(node())
                          << CAF_ARG(fail_state_));
  // send exit messages
  for (attachable* i = head.get(); i != nullptr; i = i->next.get())
    i->actor_exited(fail_state_, host);
  // tell printer to purge its state for us if we ever used aout()
  if ((fs & has_used_aout_flag) != 0) {
    auto pr = home_system().printer();
    pr->enqueue(make_mailbox_element(ctrl(), make_message_id(), delete_atom_v,
                                     id()),
                host);
  }
  unregister_from_system();
  on_cleanup(fail_state_);
  return true;
}

mailbox_element* abstract_actor::peek_at_next_mailbox_element() {
  return nullptr;
}

void abstract_actor::register_at_system() {
  if (getf(is_registered_flag))
    return;
  setf(is_registered_flag);
  [[maybe_unused]] auto count = home_system().registry().inc_running();
  CAF_LOG_DEBUG("actor" << id() << "increased running count to" << count);
}

void abstract_actor::unregister_from_system() {
  if (!getf(is_registered_flag))
    return;
  unsetf(is_registered_flag);
  [[maybe_unused]] auto count = home_system().registry().dec_running();
  CAF_LOG_DEBUG("actor" << id() << "decreased running count to" << count);
}

void abstract_actor::add_link(abstract_actor* x) {
  // Add backlink on `x` first and add the local attachable only on success.
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(x != nullptr);
  error fail_state;
  bool send_exit_immediately = false;
  auto tmp = default_attachable::make_link(address(), x->address());
  joined_exclusive_critical_section(this, x, [&] {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      send_exit_immediately = true;
    } else if (x->add_backlink(this)) {
      attach_impl(tmp);
    }
  });
  if (send_exit_immediately) {
    auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                    exit_msg{address(), fail_state});
    x->enqueue(std::move(ptr), nullptr);
  }
}

void abstract_actor::remove_link(abstract_actor* x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  default_attachable::observe_token tk{x->address(), default_attachable::link};
  joined_exclusive_critical_section(this, x, [&] {
    x->remove_backlink(this);
    detach_impl(tk, true);
  });
}

bool abstract_actor::add_backlink(abstract_actor* x) {
  // Called in an exclusive critical section.
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(x);
  error fail_state;
  bool send_exit_immediately = false;
  default_attachable::observe_token tk{x->address(), default_attachable::link};
  auto tmp = default_attachable::make_link(address(), x->address());
  auto success = false;
  if (getf(is_terminated_flag)) {
    fail_state = fail_state_;
    send_exit_immediately = true;
  } else if (detach_impl(tk, true, true) == 0) {
    attach_impl(tmp);
    success = true;
  }
  if (send_exit_immediately) {
    auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                    exit_msg{address(), fail_state});
    x->enqueue(std::move(ptr), nullptr);
  }
  return success;
}

bool abstract_actor::remove_backlink(abstract_actor* x) {
  // Called in an exclusive critical section.
  CAF_LOG_TRACE(CAF_ARG(x));
  default_attachable::observe_token tk{x->address(), default_attachable::link};
  return detach_impl(tk, true) > 0;
}

} // namespace caf
