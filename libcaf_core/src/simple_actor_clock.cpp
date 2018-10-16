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

bool simple_actor_clock::ordinary_predicate::
operator()(const secondary_map::value_type& x) const noexcept {
  auto ptr = get_if<ordinary_timeout>(&x.second->second);
  return ptr != nullptr ? ptr->type == type : false;
}

bool simple_actor_clock::multi_predicate::
operator()(const secondary_map::value_type& x) const noexcept {
  auto ptr = get_if<multi_timeout>(&x.second->second);
  return ptr != nullptr ? ptr->type == type : false;
}

bool simple_actor_clock::request_predicate::
operator()(const secondary_map::value_type& x) const noexcept {
  auto ptr = get_if<request_timeout>(&x.second->second);
  return ptr != nullptr ? ptr->id == id : false;
}

void simple_actor_clock::visitor::operator()(ordinary_timeout& x) {
  CAF_ASSERT(x.self != nullptr);
  x.self->get()->eq_impl(make_message_id(), x.self, nullptr,
                         timeout_msg{x.type, x.id});
  ordinary_predicate pred{x.type};
  thisptr->drop_lookup(x.self->get(), pred);
}

void simple_actor_clock::visitor::operator()(multi_timeout& x) {
  CAF_ASSERT(x.self != nullptr);
  x.self->get()->eq_impl(make_message_id(), x.self, nullptr,
                         timeout_msg{x.type, x.id});
  multi_predicate pred{x.type};
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
  x.target->eq_impl(make_message_id(), std::move(x.sender), nullptr,
                    std::move(x.content));
}

void simple_actor_clock::set_ordinary_timeout(time_point t, abstract_actor* self,
                                              atom_value type, uint64_t id) {
  ordinary_predicate pred{type};
  auto i = lookup(self, pred);
  auto sptr = actor_cast<strong_actor_ptr>(self);
  ordinary_timeout tmp{std::move(sptr), type, id};
  if (i != actor_lookup_.end()) {
    schedule_.erase(i->second);
    i->second = schedule_.emplace(t, std::move(tmp));
  } else {
    auto j = schedule_.emplace(t, std::move(tmp));
    actor_lookup_.emplace(self, j);
  }
}

void simple_actor_clock::set_multi_timeout(time_point t, abstract_actor* self,
                                           atom_value type, uint64_t id) {
  auto sptr = actor_cast<strong_actor_ptr>(self);
  multi_timeout tmp{std::move(sptr), type, id};
  auto j = schedule_.emplace(t, std::move(tmp));
  actor_lookup_.emplace(self, j);
}

void simple_actor_clock::set_request_timeout(time_point t, abstract_actor* self,
                                             message_id id) {
  request_predicate pred{id};
  auto i = lookup(self, pred);
  auto sptr = actor_cast<strong_actor_ptr>(self);
  request_timeout tmp{std::move(sptr), id};
  if (i != actor_lookup_.end()) {
    schedule_.erase(i->second);
    i->second = schedule_.emplace(t, std::move(tmp));
  } else {
    auto j = schedule_.emplace(t, std::move(tmp));
    actor_lookup_.emplace(self, j);
  }
}

void simple_actor_clock::cancel_ordinary_timeout(abstract_actor* self,
                                                 atom_value type) {
  ordinary_predicate pred{type};
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

void simple_actor_clock::cancel_all() {
  actor_lookup_.clear();
  schedule_.clear();
}

} // namespace detail
} // namespace caf
