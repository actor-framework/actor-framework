/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/monitorable_actor.hpp"

#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/message_handler.hpp"
#include "caf/system_messages.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {

const char* monitorable_actor::name() const {
  return "monitorable_actor";
}

void monitorable_actor::attach(attachable_ptr ptr) {
  CAF_LOG_TRACE("");
  CAF_ASSERT(ptr);
  error fail_state;
  auto attached = exclusive_critical_section([&] {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      return false;
    }
    attach_impl(ptr);
    return true;
  });
  CAF_LOG_DEBUG("cannot attach functor to terminated actor: call immediately");
  if (!attached)
    ptr->actor_exited(fail_state, nullptr);
}

size_t monitorable_actor::detach(const attachable::token& what) {
  CAF_LOG_TRACE("");
  std::unique_lock<std::mutex> guard{mtx_};
  return detach_impl(what, attachables_head_);
}

bool monitorable_actor::cleanup(error&& reason, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  attachable_ptr head;
  bool set_fail_state = exclusive_critical_section([&]() -> bool {
    if (!getf(is_cleaned_up_flag)) {
      // local actors pass fail_state_ as first argument
      if (&fail_state_ != &reason)
        fail_state_ = std::move(reason);
      attachables_head_.swap(head);
      flags(flags() | is_terminated_flag | is_cleaned_up_flag);
      on_cleanup();
      return true;
    }
    return false;
  });
  if (!set_fail_state)
    return false;
  CAF_LOG_INFO("cleanup" << CAF_ARG(id())
               << CAF_ARG(node()) << CAF_ARG(reason));
  // send exit messages
  for (attachable* i = head.get(); i != nullptr; i = i->next.get())
    i->actor_exited(reason, host);
  // tell printer to purge its state for us if we ever used aout()
  if (getf(abstract_actor::has_used_aout_flag)) {
    auto pr = home_system().scheduler().printer();
    pr->enqueue(make_mailbox_element(nullptr, message_id::make(), {},
                                      delete_atom::value, id()),
                nullptr);
  }
  return true;
}

void monitorable_actor::on_cleanup() {
  // nop
}

void monitorable_actor::bounce(mailbox_element_ptr& what) {
  error err;
  shared_critical_section([&] {
    err = fail_state_;
  });
  bounce(what, err);
}

void monitorable_actor::bounce(mailbox_element_ptr& what, const error& err) {
  // make sure that a request always gets a response;
  // the exit reason reflects the first actor on the
  // forwarding chain that is out of service
  detail::sync_request_bouncer rb{err};
  rb(*what);
}

monitorable_actor::monitorable_actor(actor_config& cfg) : abstract_actor(cfg) {
  // nop
}

bool monitorable_actor::link_impl(linking_operation op, abstract_actor* other) {
  CAF_LOG_TRACE(CAF_ARG(op) << CAF_ARG(other));
  switch (op) {
    case establish_link_op:
      return establish_link_impl(other);
    case establish_backlink_op:
      return establish_backlink_impl(other);
    case remove_link_op:
      return remove_link_impl(other);
    default:
      CAF_ASSERT(op == remove_backlink_op);
      return remove_backlink_impl(other);
  }
}

bool monitorable_actor::establish_link_impl(abstract_actor* x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(x);
  error fail_state;
  bool send_exit_immediately = false;
  auto tmp = default_attachable::make_link(address(), x->address());
  auto success = exclusive_critical_section([&]() -> bool {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      send_exit_immediately = true;
      return false;
    }
    // add link if not already linked to other (checked by establish_backlink)
    if (x->establish_backlink(this)) {
      attach_impl(tmp);
      return true;
    }
    return false;
  });
  if (send_exit_immediately)
    x->enqueue(nullptr, invalid_message_id,
                 make_message(exit_msg{address(), fail_state}), nullptr);
  return success;
}

bool monitorable_actor::establish_backlink_impl(abstract_actor* x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(x);
  error fail_state;
  bool send_exit_immediately = false;
  default_attachable::observe_token tk{x->address(),
                                       default_attachable::link};
  auto tmp = default_attachable::make_link(address(), x->address());
  auto success = exclusive_critical_section([&]() -> bool {
    if (getf(is_terminated_flag)) {
      fail_state = fail_state_;
      send_exit_immediately = true;
      return false;
    }
    if (detach_impl(tk, attachables_head_, true, true) == 0) {
      attach_impl(tmp);
      return true;
    }
    return false;
  });
  if (send_exit_immediately)
    x->enqueue(nullptr, invalid_message_id,
                 make_message(exit_msg{address(), fail_state}), nullptr);
  return success;
}

bool monitorable_actor::remove_link_impl(abstract_actor* x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  default_attachable::observe_token tk{x->address(), default_attachable::link};
  auto success = exclusive_critical_section([&]() -> bool {
    return detach_impl(tk, attachables_head_, true) > 0;
  });
  if (success)
    x->remove_backlink(this);
  return success;
}

bool monitorable_actor::remove_backlink_impl(abstract_actor* x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  default_attachable::observe_token tk{x->address(), default_attachable::link};
  auto success = exclusive_critical_section([&]() -> bool {
    return detach_impl(tk, attachables_head_, true) > 0;
  });
  return success;
}

size_t monitorable_actor::detach_impl(const attachable::token& what,
                                      attachable_ptr& ptr, bool stop_on_hit,
                                      bool dry_run) {
  CAF_LOG_TRACE("");
  if (!ptr) {
    CAF_LOG_DEBUG("invalid ptr");
    return 0;
  }
  if (ptr->matches(what)) {
    if (!dry_run) {
      CAF_LOG_DEBUG("removed element");
      attachable_ptr next;
      next.swap(ptr->next);
      ptr.swap(next);
    }
    return stop_on_hit ? 1 : 1 + detach_impl(what, ptr, stop_on_hit, dry_run);
  }
  return detach_impl(what, ptr->next, stop_on_hit, dry_run);
}

bool monitorable_actor::handle_system_message(mailbox_element& x,
                                              execution_unit* ctx,
                                              bool trap_exit) {
  auto& msg = x.content();
  if (!trap_exit && msg.size() == 1 && msg.match_element<exit_msg>(0)) {
    // exits for non-normal exit reasons
    auto& em = msg.get_mutable_as<exit_msg>(0);
    if (em.reason)
      cleanup(std::move(em.reason), ctx);
    return true;
  }
  if (msg.size() > 1 && msg.match_element<sys_atom>(0)) {
    if (!x.sender)
      return true;
    error err;
    mailbox_element_ptr res;
    msg.apply(
      [&](sys_atom, get_atom, std::string& what) {
        CAF_LOG_TRACE(CAF_ARG(what));
        if (what != "info") {
          err = sec::unsupported_sys_key;
          return;
        }
        res = make_mailbox_element(ctrl(), x.mid.response_id(), {},
                                    ok_atom::value, std::move(what),
                                    strong_actor_ptr{ctrl()}, name());
      }
    );
    if (!res && !err)
      err = sec::unsupported_sys_message;
    if (err && x.mid.is_request())
      res = make_mailbox_element(ctrl(), x.mid.response_id(),
                                  {}, std::move(err));
    if (res) {
      auto s = actor_cast<strong_actor_ptr>(x.sender);
      if (s)
        s->enqueue(std::move(res), ctx);
    }
    return true;
  }
  return false;
}

} // namespace caf
