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

#include "caf/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/system_messages.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {

namespace {
using guard_type = std::unique_lock<std::mutex>;
} // namespace <anonymous>

// exit_reason_ is guaranteed to be set to 0, i.e., exit_reason::not_exited,
// by std::atomic<> constructor

abstract_actor::abstract_actor(actor_id aid, node_id nid)
    : abstract_channel(abstract_channel::is_abstract_actor_flag,
                       std::move(nid)),
      id_(aid),
      exit_reason_(exit_reason::not_exited),
      host_(nullptr) {
  // nop
}

abstract_actor::abstract_actor()
    : abstract_channel(abstract_channel::is_abstract_actor_flag,
                       detail::singletons::get_node_id()),
      id_(detail::singletons::get_actor_registry()->next_id()),
      exit_reason_(exit_reason::not_exited),
      host_(nullptr) {
  // nop
}

void abstract_actor::attach(attachable_ptr ptr) {
  CAF_LOG_TRACE("");
  if (ptr == nullptr) {
    return;
  }
  uint32_t reason;
  { // lifetime scope of guard
    guard_type guard{mtx_};
    reason = exit_reason_;
    if (reason == exit_reason::not_exited) {
      attach_impl(ptr);
      return;
    }
  }
  CAF_LOG_DEBUG("cannot attach functor to terminated actor: call immediately");
  ptr->actor_exited(this, reason);
}

size_t abstract_actor::detach_impl(const attachable::token& what,
                                   attachable_ptr& ptr,
                                   bool stop_on_hit,
                                   bool dry_run) {
  CAF_LOGF_TRACE("");
  if (! ptr) {
    CAF_LOGF_DEBUG("invalid ptr");
    return 0;
  }
  if (ptr->matches(what)) {
    if (! dry_run) {
      CAF_LOGF_DEBUG("removed element");
      attachable_ptr next;
      next.swap(ptr->next);
      ptr.swap(next);
    }
    return stop_on_hit ? 1 : 1 + detach_impl(what, ptr, stop_on_hit, dry_run);
  }
  return detach_impl(what, ptr->next, stop_on_hit, dry_run);
}

size_t abstract_actor::detach(const attachable::token& what) {
  CAF_LOG_TRACE("");
  guard_type guard{mtx_};
  return detach_impl(what, attachables_head_);
}

void abstract_actor::is_registered(bool value) {
  if (is_registered() == value) {
    return;
  }
  if (value) {
    detail::singletons::get_actor_registry()->inc_running();
  } else {
    detail::singletons::get_actor_registry()->dec_running();
  }
  set_flag(value, is_registered_flag);
}

bool abstract_actor::link_impl(linking_operation op, const actor_addr& other) {
  CAF_LOG_TRACE(CAF_ARG(op) << ", " << CAF_TSARG(other));
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

bool abstract_actor::establish_link_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_TSARG(other));
  if (other && other != this) {
    guard_type guard{mtx_};
    auto ptr = actor_cast<abstract_actor_ptr>(other);
    // send exit message if already exited
    if (exited()) {
      ptr->enqueue(address(), invalid_message_id,
                   make_message(exit_msg{address(), exit_reason()}), host_);
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

bool abstract_actor::establish_backlink_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_TSARG(other));
  uint32_t reason = exit_reason::not_exited;
  default_attachable::observe_token tk{other, default_attachable::link};
  if (other && other != this) {
    guard_type guard{mtx_};
    reason = exit_reason_;
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
                 make_message(exit_msg{address(), exit_reason()}), host_);
  }
  return false;
}

bool abstract_actor::remove_link_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_TSARG(other));
  if (other == invalid_actor_addr || other == this) {
    return false;
  }
  default_attachable::observe_token tk{other, default_attachable::link};
  guard_type guard{mtx_};
  // remove_backlink returns true if this actor is linked to other
  auto ptr = actor_cast<abstract_actor_ptr>(other);
  if (detach_impl(tk, attachables_head_, true) > 0) {
    // tell remote side to remove link as well
    ptr->remove_backlink(address());
    return true;
  }
  return false;
}

bool abstract_actor::remove_backlink_impl(const actor_addr& other) {
  CAF_LOG_TRACE(CAF_TSARG(other));
  default_attachable::observe_token tk{other, default_attachable::link};
  if (other && other != this) {
    guard_type guard{mtx_};
    return detach_impl(tk, attachables_head_, true) > 0;
  }
  return false;
}

actor_addr abstract_actor::address() const {
  return actor_addr{const_cast<abstract_actor*>(this)};
}

void abstract_actor::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  CAF_ASSERT(reason != exit_reason::not_exited);
  // move everyhting out of the critical section before processing it
  attachable_ptr head;
  { // lifetime scope of guard
    guard_type guard{mtx_};
    if (exit_reason_ != exit_reason::not_exited) {
      // already exited
      return;
    }
    exit_reason_ = reason;
    attachables_head_.swap(head);
  }
  CAF_LOG_INFO_IF(! is_remote(), "cleanup actor with ID " << id_
                                << "; exit reason = " << reason);
  // send exit messages
  for (attachable* i = head.get(); i != nullptr; i = i->next.get()) {
    i->actor_exited(this, reason);
  }
}

std::set<std::string> abstract_actor::message_types() const {
  // defaults to untyped
  return std::set<std::string>{};
}

optional<uint32_t> abstract_actor::handle(const std::exception_ptr& eptr) {
  { // lifetime scope of guard
    guard_type guard{mtx_};
    for (auto i = attachables_head_.get(); i != nullptr; i = i->next.get()) {
      try {
        auto result = i->handle_exception(eptr);
        if (result) {
          return *result;
        }
      }
      catch (...) {
        // ignore exceptions
      }
    }
  }
  return none;
}

} // namespace caf
