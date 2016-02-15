/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/on.hpp"
#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/message_handler.hpp"
#include "caf/system_messages.hpp"
#include "caf/default_attachable.hpp"

namespace caf {

const char* monitorable_actor::name() const {
  return "monitorable_actor";
}

void monitorable_actor::attach(attachable_ptr ptr) {
  CAF_LOG_TRACE("");
  if (ptr == nullptr)
    return;
  caf::exit_reason reason;
  { // lifetime scope of guard
    std::unique_lock<std::mutex> guard{mtx_};
    reason = get_exit_reason();
    if (reason == exit_reason::not_exited) {
      attach_impl(ptr);
      return;
    }
  }
  CAF_LOG_DEBUG("cannot attach functor to terminated actor: call immediately");
  ptr->actor_exited(this, reason, nullptr);
}

size_t monitorable_actor::detach(const attachable::token& what) {
  CAF_LOG_TRACE("");
  std::unique_lock<std::mutex> guard{mtx_};
  return detach_impl(what, attachables_head_);
}

void monitorable_actor::cleanup(exit_reason reason, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  CAF_ASSERT(reason != exit_reason::not_exited);
  // move everything out of the critical section before processing it
  attachable_ptr head;
  { // lifetime scope of guard
    std::unique_lock<std::mutex> guard{mtx_};
    if (get_exit_reason() != exit_reason::not_exited)
      return;
    exit_reason_.store(reason, std::memory_order_relaxed);
    attachables_head_.swap(head);
  }
  CAF_LOG_INFO_IF(host && host->system().node() == node(),
                  "cleanup" << CAF_ARG(id()) << CAF_ARG(reason));
  // send exit messages
  for (attachable* i = head.get(); i != nullptr; i = i->next.get())
    i->actor_exited(this, reason, host);
}

monitorable_actor::monitorable_actor(actor_config& cfg)
    : abstract_actor(cfg),
      exit_reason_(exit_reason::not_exited) {
  // nop
}

monitorable_actor::monitorable_actor(actor_system* sys,
                                     actor_id aid,
                                     node_id nid)
    : abstract_actor(sys, aid, nid),
      exit_reason_(exit_reason::not_exited) {
  // nop
}

monitorable_actor::monitorable_actor(actor_system* sys, actor_id aid,
                                     node_id nid, int init_flags)
    : abstract_actor(sys, aid, nid, init_flags),
      exit_reason_(exit_reason::not_exited) {
  // nop
}

maybe<exit_reason> monitorable_actor::handle(const std::exception_ptr& eptr) {
  std::unique_lock<std::mutex> guard{mtx_};
  for (auto i = attachables_head_.get(); i != nullptr; i = i->next.get()) {
    try {
      auto result = i->handle_exception(eptr);
      if (result)
        return *result;
    }
    catch (...) {
      // ignore exceptions
    }
  }
  return none;
}

bool monitorable_actor::link_impl(linking_operation op, const actor_addr& other) {
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

bool monitorable_actor::establish_link_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(other));
  if (other && other != this) {
    std::unique_lock<std::mutex> guard{mtx_};
    auto ptr = actor_cast<abstract_actor_ptr>(other);
    auto reason = get_exit_reason();
    // send exit message if already exited
    if (reason != exit_reason::not_exited) {
      guard.unlock();
      ptr->enqueue(address(), invalid_message_id,
                   make_message(exit_msg{address(), reason}), nullptr);
    } else if (ptr->establish_backlink(address())) {
      // add link if not already linked to other
      // (checked by establish_backlink)
      auto tmp = default_attachable::make_link(other);
      attach_impl(tmp);
      return true;
    }
  }
  return false;
}

bool monitorable_actor::establish_backlink_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(other));
  auto reason = exit_reason::not_exited;
  default_attachable::observe_token tk{other, default_attachable::link};
  if (other && other != this) {
    std::unique_lock<std::mutex> guard{mtx_};
    reason = get_exit_reason();
    if (reason == exit_reason::not_exited) {
      if (detach_impl(tk, attachables_head_, true, true) == 0) {
        auto tmp = default_attachable::make_link(other);
        attach_impl(tmp);
        return true;
      }
    }
  }
  // send exit message without lock
  if (reason != exit_reason::not_exited) {
    auto ptr = actor_cast<abstract_actor_ptr>(other);
    ptr->enqueue(address(), invalid_message_id,
                 make_message(exit_msg{address(), reason}), nullptr);
  }
  return false;
}

bool monitorable_actor::remove_link_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(other));
  if (other == invalid_actor_addr || other == this)
    return false;
  default_attachable::observe_token tk{other, default_attachable::link};
  std::unique_lock<std::mutex> guard{mtx_};
  // remove_backlink returns true if this actor is linked to other
  auto ptr = actor_cast<abstract_actor_ptr>(other);
  if (detach_impl(tk, attachables_head_, true) > 0) {
    guard.unlock();
    // tell remote side to remove link as well
    ptr->remove_backlink(address());
    return true;
  }
  return false;
}

bool monitorable_actor::remove_backlink_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(other));
  default_attachable::observe_token tk{other, default_attachable::link};
  if (other && other != this) {
    std::unique_lock<std::mutex> guard{mtx_};
    return detach_impl(tk, attachables_head_, true) > 0;
  }
  return false;
}

size_t monitorable_actor::detach_impl(const attachable::token& what,
                                      attachable_ptr& ptr, bool stop_on_hit,
                                      bool dry_run) {
  CAF_LOG_TRACE("");
  if (! ptr) {
    CAF_LOG_DEBUG("invalid ptr");
    return 0;
  }
  if (ptr->matches(what)) {
    if (! dry_run) {
      CAF_LOG_DEBUG("removed element");
      attachable_ptr next;
      next.swap(ptr->next);
      ptr.swap(next);
    }
    return stop_on_hit ? 1 : 1 + detach_impl(what, ptr, stop_on_hit, dry_run);
  }
  return detach_impl(what, ptr->next, stop_on_hit, dry_run);
}

bool monitorable_actor::handle_system_message(mailbox_element& node,
                                              execution_unit* context,
                                              bool trap_exit) {
  auto& msg = node.msg;
  if (! trap_exit && msg.size() == 1 && msg.match_element<exit_msg>(0)) {
    // exits for non-normal exit reasons
    auto& em = msg.get_as<exit_msg>(0);
    CAF_ASSERT(em.reason != exit_reason::not_exited);
    if (em.reason != exit_reason::normal)
      cleanup(em.reason, context);
    return true;
  } else if (msg.size() > 1 && msg.match_element<sys_atom>(0)) {
    if (! node.sender)
      return true;
    error err;
    mailbox_element_ptr res;
    msg.apply({
      [&](sys_atom, get_atom, std::string& what) {
        CAF_LOG_TRACE(CAF_ARG(what));
        if (what != "info") {
          err = sec::invalid_sys_key;
          return;
        }
        res = mailbox_element::make_joint(address(), node.mid.response_id(), {},
                                          ok_atom::value, std::move(what),
                                          address(), name());
      },
      others >> [&] {
        err = sec::unsupported_sys_message;
      }
    });
    if (err && node.mid.is_request())
      res = mailbox_element::make_joint(address(), node.mid.response_id(),
                                        {}, std::move(err));
    if (res)
      node.sender->enqueue(std::move(res), context);
    return true;
  }
  return false;
}

} // namespace caf
