/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <string>
#include "caf/all.hpp"
#include "caf/atom.hpp"
#include "caf/actor_cast.hpp"
#include "caf/local_actor.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/logging.hpp"

namespace caf {

// local actors are created with a reference count of one that is adjusted
// later on in spawn(); this prevents subtle bugs that lead to segfaults,
// e.g., when calling address() in the ctor of a derived class
local_actor::local_actor()
    : super(1),
      m_dummy_node(),
      m_current_node(&m_dummy_node),
      m_planned_exit_reason(exit_reason::not_exited) {
  // nop
}

local_actor::~local_actor() {
  // nop
}

void local_actor::monitor(const actor_addr& whom) {
  if (whom == invalid_actor_addr) {
    return;
  }
  auto ptr = actor_cast<abstract_actor_ptr>(whom);
  ptr->attach(default_attachable::make_monitor(address()));
}

void local_actor::demonitor(const actor_addr& whom) {
  if (whom == invalid_actor_addr) {
    return;
  }
  auto ptr = actor_cast<abstract_actor_ptr>(whom);
  default_attachable::observe_token tk{address(), default_attachable::monitor};
  ptr->detach({typeid(default_attachable::observe_token), &tk});
}

void local_actor::join(const group& what) {
  CAF_LOG_TRACE(CAF_TSARG(what));
  if (what == invalid_group) {
    return;
  }
  abstract_group::subscription_token tk{what.ptr()};
  std::unique_lock<std::mutex> guard{m_mtx};
  if (detach_impl(tk, m_attachables_head, true, true) == 0) {
    auto ptr = what->subscribe(address());
    if (ptr) {
      attach_impl(ptr);
    }
  }
}

void local_actor::leave(const group& what) {
  CAF_LOG_TRACE(CAF_TSARG(what));
  if (what == invalid_group) {
    return;
  }
  if (detach(abstract_group::subscription_token{what.ptr()}) > 0) {
    what->unsubscribe(address());
  }
}

std::vector<group> local_actor::joined_groups() const {
  std::vector<group> result;
  result.reserve(20);
  std::unique_lock<std::mutex> guard{m_mtx};
  for (attachable* i = m_attachables_head.get(); i != 0; i = i->next.get()) {
    auto sptr = dynamic_cast<abstract_group::subscription*>(i);
    if (sptr) {
      result.emplace_back(sptr->group());
    }
  }
  return result;
}

void local_actor::reply_message(message&& what) {
  auto& whom = m_current_node->sender;
  if (!whom) {
    return;
  }
  auto& id = m_current_node->mid;
  if (id.valid() == false || id.is_response()) {
    send_tuple(actor_cast<channel>(whom), std::move(what));
  } else if (!id.is_answered()) {
    auto ptr = actor_cast<actor>(whom);
    ptr->enqueue(address(), id.response_id(), std::move(what), host());
    id.mark_as_answered();
  }
}

void local_actor::forward_message(const actor& dest, message_priority prio) {
  if (!dest) {
    return;
  }
  auto id = (prio == message_priority::high)
              ? m_current_node->mid.with_high_priority()
              : m_current_node->mid.with_normal_priority();
  dest->enqueue(m_current_node->sender, id, m_current_node->msg, host());
  // treat this message as asynchronous message from now on
  m_current_node->mid = invalid_message_id;
}

void local_actor::send_tuple(message_priority prio, const channel& dest,
                             message what) {
  if (!dest) {
    return;
  }
  message_id id;
  if (prio == message_priority::high) {
    id = id.with_high_priority();
  }
  dest->enqueue(address(), id, std::move(what), host());
}

void local_actor::send_exit(const actor_addr& whom, uint32_t reason) {
  send(actor_cast<actor>(whom), exit_msg{address(), reason});
}

void local_actor::delayed_send_tuple(message_priority prio, const channel& dest,
                                     const duration& rel_time, message msg) {
  message_id mid;
  if (prio == message_priority::high) {
    mid = mid.with_high_priority();
  }
  auto sched_cd = detail::singletons::get_scheduling_coordinator();
  sched_cd->delayed_send(rel_time, address(), dest, mid, std::move(msg));
}

response_promise local_actor::make_response_promise() {
  auto n = m_current_node;
  response_promise result{address(), n->sender, n->mid.response_id()};
  n->mid.mark_as_answered();
  return result;
}

void local_actor::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  super::cleanup(reason);
  // tell registry we're done
  is_registered(false);
}

void local_actor::quit(uint32_t reason) {
  CAF_LOG_TRACE("reason = " << reason << ", class "
                            << detail::demangle(typeid(*this)));
  planned_exit_reason(reason);
  if (is_blocking()) {
    throw actor_exited(reason);
  }
}

message_id local_actor::timed_sync_send_tuple_impl(message_priority mp,
                                                   const actor& dest,
                                                   const duration& rtime,
                                                   message&& what) {
  if (!dest) {
    throw std::invalid_argument(
      "cannot send synchronous message "
      "to invalid_actor");
  }
  auto nri = new_request_id();
  if (mp == message_priority::high) {
    nri = nri.with_high_priority();
  }
  dest->enqueue(address(), nri, std::move(what), host());
  auto rri = nri.response_id();
  auto sched_cd = detail::singletons::get_scheduling_coordinator();
  sched_cd->delayed_send(rtime, address(), this, rri,
                         make_message(sync_timeout_msg{}));
  return rri;
}

message_id local_actor::sync_send_tuple_impl(message_priority mp,
                                             const actor& dest,
                                             message&& what) {
  if (!dest) {
    throw std::invalid_argument(
      "cannot send synchronous message "
      "to invalid_actor");
  }
  auto nri = new_request_id();
  if (mp == message_priority::high) {
    nri = nri.with_high_priority();
  }
  dest->enqueue(address(), nri, std::move(what), host());
  return nri.response_id();
}

void local_actor::is_registered(bool value) {
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

} // namespace caf
