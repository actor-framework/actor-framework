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

#include "caf/actor_pool.hpp"

#include "caf/send.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {

actor_pool::round_robin::round_robin() : m_pos(0) {
  // nop
}

actor_pool::round_robin::round_robin(const round_robin&) : m_pos(0) {
  // nop
}

void actor_pool::round_robin::operator()(uplock& guard, const actor_vec& vec,
                                         mailbox_element_ptr& ptr,
                                         execution_unit* host) {
  CAF_ASSERT(!vec.empty());
  actor selected = vec[m_pos++ % vec.size()];
  guard.unlock();
  selected->enqueue(std::move(ptr), host);
}

void actor_pool::broadcast::operator()(uplock&, const actor_vec& vec,
                                       mailbox_element_ptr& ptr,
                                       execution_unit* host) {
  CAF_ASSERT(!vec.empty());
  for (size_t i = 1; i < vec.size(); ++i) {
    vec[i]->enqueue(ptr->sender, ptr->mid, ptr->msg, host);
  }
  vec.front()->enqueue(std::move(ptr), host);
}

actor_pool::random::random() {
  // nop
}

actor_pool::random::random(const random&) {
  // nop
}

void actor_pool::random::operator()(uplock& guard, const actor_vec& vec,
                                    mailbox_element_ptr& ptr,
                                    execution_unit* host) {
  std::uniform_int_distribution<size_t> dis(0, vec.size() - 1);
  upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
  actor selected = vec[dis(m_rd)];
  unique_guard.unlock();
  selected->enqueue(std::move(ptr), host);
}

actor_pool::~actor_pool() {
  // nop
}

actor actor_pool::make(policy pol) {
  intrusive_ptr<actor_pool> ptr;
  ptr = make_counted<actor_pool>();
  ptr->m_policy = std::move(pol);
  return actor_cast<actor>(ptr);
}

actor actor_pool::make(size_t num_workers, factory fac, policy pol) {
  auto res = make(std::move(pol));
  auto ptr = static_cast<actor_pool*>(actor_cast<abstract_actor*>(res));
  auto res_addr = ptr->address();
  for (size_t i = 0; i < num_workers; ++i) {
    auto worker = fac();
    worker->attach(default_attachable::make_monitor(res_addr));
    ptr->m_workers.push_back(worker);
  }
  return res;
}

void actor_pool::enqueue(const actor_addr& sender, message_id mid,
                         message content, execution_unit* eu) {
  upgrade_lock<detail::shared_spinlock> guard{m_mtx};
  if (filter(guard, sender, mid, content, eu)) {
    return;
  }
  auto ptr = mailbox_element::make(sender, mid, std::move(content));
  m_policy(guard, m_workers, ptr, eu);
}

void actor_pool::enqueue(mailbox_element_ptr what, execution_unit* eu) {
  upgrade_lock<detail::shared_spinlock> guard{m_mtx};
  if (filter(guard, what->sender, what->mid, what->msg, eu)) {
    return;
  }
  m_policy(guard, m_workers, what, eu);
}

actor_pool::actor_pool() : m_planned_reason(caf::exit_reason::not_exited) {
  is_registered(true);
}

bool actor_pool::filter(upgrade_lock<detail::shared_spinlock>& guard,
                        const actor_addr& sender, message_id mid,
                        const message& msg, execution_unit* eu) {
  auto rsn = m_planned_reason;
  if (rsn != caf::exit_reason::not_exited) {
    guard.unlock();
    if (mid.valid()) {
      detail::sync_request_bouncer srq{rsn};
      srq(sender, mid);
    }
    return true;
  }
  if (msg.match_elements<exit_msg>()) {
    std::vector<actor> workers;
    // send exit messages *always* to all workers and clear vector afterwards
    // but first swap m_workers out of the critical section
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    m_workers.swap(workers);
    m_planned_reason = msg.get_as<exit_msg>(0).reason;
    unique_guard.unlock();
    for (auto& w : workers) {
      anon_send(w, msg);
    }
    quit();
    return true;
  }
  if (msg.match_elements<down_msg>()) {
    // remove failed worker from pool
    auto& dm = msg.get_as<down_msg>(0);
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    auto last = m_workers.end();
    auto i = std::find(m_workers.begin(), m_workers.end(), dm.source);
    if (i != last) {
      m_workers.erase(i);
    }
    if (m_workers.empty()) {
      m_planned_reason = exit_reason::out_of_workers;
      unique_guard.unlock();
      quit();
    }
    return true;
  }
  if (msg.match_elements<sys_atom, put_atom, actor>()) {
    auto& worker = msg.get_as<actor>(2);
    if (worker == invalid_actor) {
      return true;
    }
    worker->attach(default_attachable::make_monitor(address()));
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    m_workers.push_back(worker);
    return true;
  }
  if (msg.match_elements<sys_atom, delete_atom, actor>()) {
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    auto& what = msg.get_as<actor>(2);
    auto last = m_workers.end();
    auto i = std::find(m_workers.begin(), last, what);
    if (i != last) {
      m_workers.erase(i);
    }
    return true;
  }
  if (msg.match_elements<sys_atom, get_atom>()) {
    auto cpy = m_workers;
    guard.unlock();
    actor_cast<abstract_actor*>(sender)->enqueue(invalid_actor_addr,
                                                 mid.response_id(),
                                                 make_message(std::move(cpy)),
                                                 eu);
    return true;
  }
  if (m_workers.empty()) {
    guard.unlock();
    if (sender != invalid_actor_addr && mid.valid()) {
      // tell client we have ignored this sync message by sending
      // and empty message back
      auto ptr = actor_cast<abstract_actor_ptr>(sender);
      ptr->enqueue(invalid_actor_addr, mid.response_id(), message{}, eu);
    }
    return true;
  }
  return false;
}

void actor_pool::quit() {
  // we can safely run our cleanup code here without holding
  // m_mtx because abstract_actor has its own lock
  cleanup(m_planned_reason);
  is_registered(false);
}

} // namespace caf
