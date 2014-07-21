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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <string>
#include "caf/all.hpp"
#include "caf/atom.hpp"
#include "caf/scheduler.hpp"
#include "caf/actor_cast.hpp"
#include "caf/local_actor.hpp"

#include "caf/detail/logging.hpp"

namespace caf {

namespace {

class down_observer : public attachable {

 public:

  down_observer(actor_addr observer, actor_addr observed)
      : m_observer(std::move(observer)), m_observed(std::move(observed)) {
    CAF_REQUIRE(m_observer != invalid_actor_addr);
    CAF_REQUIRE(m_observed != invalid_actor_addr);
  }

  void actor_exited(uint32_t reason) {
    auto ptr = actor_cast<abstract_actor_ptr>(m_observer);
    ptr->enqueue(m_observed, message_id {}.with_high_priority(),
           make_message(down_msg{m_observed, reason}), nullptr);
  }

  bool matches(const attachable::token& match_token) {
    if (match_token.subtype == typeid(down_observer)) {
      auto ptr = reinterpret_cast<const local_actor*>(match_token.ptr);
      return m_observer == ptr->address();
    }
    return false;
  }

 private:

  actor_addr m_observer;
  actor_addr m_observed;

};

} // namespace <anonymous>

local_actor::local_actor()
    : m_trap_exit(false)
    , m_dummy_node()
    , m_current_node(&m_dummy_node)
    , m_planned_exit_reason(exit_reason::not_exited) {}

local_actor::~local_actor() {}

void local_actor::monitor(const actor_addr& whom) {
  if (!whom) return;
  auto ptr = actor_cast<abstract_actor_ptr>(whom);
  ptr->attach(attachable_ptr{new down_observer(address(), whom)});
}

void local_actor::demonitor(const actor_addr& whom) {
  if (!whom) return;
  auto ptr = actor_cast<abstract_actor_ptr>(whom);
  attachable::token mtoken{typeid(down_observer), this};
  ptr->detach(mtoken);
}

void local_actor::on_exit() {}

void local_actor::join(const group& what) {
  CAF_LOG_TRACE(CAF_TSARG(what));
  if (what && m_subscriptions.count(what) == 0) {
    CAF_LOG_DEBUG("join group: " << to_string(what));
    m_subscriptions.insert(std::make_pair(what, what->subscribe(this)));
  }
}

void local_actor::leave(const group& what) {
  if (what) m_subscriptions.erase(what);
}

std::vector<group> local_actor::joined_groups() const {
  std::vector<group> result;
  for (auto& kvp : m_subscriptions) {
    result.emplace_back(kvp.first);
  }
  return result;
}

void local_actor::reply_message(message&& what) {
  auto& whom = m_current_node->sender;
  if (!whom) return;
  auto& id = m_current_node->mid;
  if (id.valid() == false || id.is_response()) {
    send_tuple(actor_cast<channel>(whom), std::move(what));
  } else if (!id.is_answered()) {
    auto ptr = actor_cast<actor>(whom);
    ptr->enqueue(address(), id.response_id(), std::move(what), m_host);
    id.mark_as_answered();
  }
}

void local_actor::forward_message(const actor& dest, message_priority prio) {
  if (!dest) return;
  auto id = (prio == message_priority::high) ?
          m_current_node->mid.with_high_priority() :
          m_current_node->mid.with_normal_priority();
  dest->enqueue(m_current_node->sender, id, m_current_node->msg, m_host);
  // treat this message as asynchronous message from now on
  m_current_node->mid = message_id::invalid;
}

void local_actor::send_tuple(message_priority prio, const channel& dest,
               message what) {
  if (!dest) return;
  message_id id;
  if (prio == message_priority::high) id = id.with_high_priority();
  dest->enqueue(address(), id, std::move(what), m_host);
}

void local_actor::send_exit(const actor_addr& whom, uint32_t reason) {
  send(actor_cast<actor>(whom), exit_msg{address(), reason});
}

void local_actor::delayed_send_tuple(message_priority prio, const channel& dest,
                   const duration& rel_time, message msg) {
  message_id mid;
  if (prio == message_priority::high) mid = mid.with_high_priority();
  detail::singletons::get_scheduling_coordinator()->delayed_send(
    rel_time, address(), dest, mid, std::move(msg));
}

response_promise local_actor::make_response_promise() {
  auto n = m_current_node;
  response_promise result{address(), n->sender, n->mid.response_id()};
  n->mid.mark_as_answered();
  return result;
}

void local_actor::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  m_subscriptions.clear();
  super::cleanup(reason);
}

void local_actor::quit(uint32_t reason) {
  CAF_LOG_TRACE("reason = " << reason << ", class "
                 << detail::demangle(typeid(*this)));
  if (reason == exit_reason::unallowed_function_call) {
    // this is the only reason that causes an exception
    cleanup(reason);
    CAF_LOG_WARNING("actor tried to use a blocking function");
    // when using receive(), the non-blocking nature of event-based
    // actors breaks any assumption the user has about his code,
    // in particular, receive_loop() is a deadlock when not throwing
    // an exception here
    aout(this) << "*** warning: event-based actor killed because it tried "
            "to use receive()\n";
    throw actor_exited(reason);
  }
  planned_exit_reason(reason);
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
  if (mp == message_priority::high) nri = nri.with_high_priority();
  dest->enqueue(address(), nri, std::move(what), m_host);
  auto rri = nri.response_id();
  detail::singletons::get_scheduling_coordinator()->delayed_send(
    rtime, address(), this, rri, make_message(sync_timeout_msg{}));
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
  if (mp == message_priority::high) nri = nri.with_high_priority();
  dest->enqueue(address(), nri, std::move(what), m_host);
  return nri.response_id();
}

void anon_send_exit(const actor_addr& whom, uint32_t reason) {
  if (!whom) return;
  auto ptr = actor_cast<actor>(whom);
  ptr->enqueue(invalid_actor_addr, message_id {}.with_high_priority(),
         make_message(exit_msg{invalid_actor_addr, reason}), nullptr);
}

} // namespace caf
