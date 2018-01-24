/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/simple_actor_clock.hpp"

#include "caf/actor_cast.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"

namespace caf {
namespace detail {

bool simple_actor_clock::receive_predicate::
operator()(const secondary_map::value_type& x) const noexcept {
  return holds_alternative<receive_timeout>(x.second->second);
}

bool simple_actor_clock::request_predicate::
operator()(const secondary_map::value_type& x) const noexcept {
  if (holds_alternative<request_timeout>(x.second->second)) {
    auto& rt = get<request_timeout>(x.second->second);
    return rt.id == id;
  }
  return false;
}

void simple_actor_clock::visitor::operator()(receive_timeout& x) {
  CAF_ASSERT(x.self != nullptr);
  x.self->get()->eq_impl(message_id::make(), x.self, nullptr,
                         timeout_msg{x.id});
  receive_predicate pred;
  thisptr->drop_lookup(x.self->get(), pred);
}

void simple_actor_clock::visitor::operator()(request_timeout& x) {
  CAF_ASSERT(x.self != nullptr);
  x.self->get()->eq_impl(x.id, x.self, nullptr, sec::request_timeout);
  request_predicate pred{x.id};
  thisptr->drop_lookup(x.self->get(), pred);
}

void simple_actor_clock::visitor::operator()(actor_msg& x) {
  x.receiver->enqueue(std::move(x.content), nullptr);
}

void simple_actor_clock::visitor::operator()(group_msg& x) {
  x.target->eq_impl(message_id::make(), std::move(x.sender), nullptr,
                    std::move(x.content));
}

void simple_actor_clock::set_receive_timeout(time_point t, abstract_actor* self,
                                             uint32_t id) {
  receive_predicate pred;
  auto i = lookup(self, pred);
  auto sptr = actor_cast<strong_actor_ptr>(self);
  if (i != actor_lookup_.end()) {
    schedule_.erase(i->second);
    i->second = schedule_.emplace(t, receive_timeout{std::move(sptr), id});
  } else {
    auto j = schedule_.emplace(t, receive_timeout{std::move(sptr), id});
    actor_lookup_.emplace(self, j);
  }
}

void simple_actor_clock::set_request_timeout(time_point t, abstract_actor* self,
                                             message_id id) {
  request_predicate pred{id};
  auto i = lookup(self, pred);
  auto sptr = actor_cast<strong_actor_ptr>(self);
  if (i != actor_lookup_.end()) {
    schedule_.erase(i->second);
    i->second = schedule_.emplace(t, request_timeout{std::move(sptr), id});
  } else {
    auto j = schedule_.emplace(t, request_timeout{std::move(sptr), id});
    actor_lookup_.emplace(self, j);
  }
}

void simple_actor_clock::cancel_receive_timeout(abstract_actor* self) {
  receive_predicate pred;
  cancel(self, pred);
}

void simple_actor_clock::cancel_request_timeout(abstract_actor* self,
                                                message_id id) {
  request_predicate pred{id};
  cancel(self, pred);
}

void simple_actor_clock::cancel_timeouts(abstract_actor* self) {
  auto range = actor_lookup_.equal_range(self);
  if (range.first == range.second)
    return;
  for (auto i = range.first; i != range.second; ++i)
    schedule_.erase(i->second);
  actor_lookup_.erase(range.first, range.second);
}

void simple_actor_clock::schedule_message(time_point t,
                                          strong_actor_ptr receiver,
                                          mailbox_element_ptr content) {
  schedule_.emplace(t, actor_msg{std::move(receiver), std::move(content)});
}

void simple_actor_clock::schedule_message(time_point t, group target,
                                          strong_actor_ptr sender,
                                          message content) {
  schedule_.emplace(
    t, group_msg{std::move(target), std::move(sender), std::move(content)});
}

} // namespace detail
} // namespace caf
