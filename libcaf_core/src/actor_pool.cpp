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

#include <atomic>
#include <random>

#include "caf/send.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {

actor_pool::policy actor_pool::round_robin() {
  struct impl {
    impl() : pos_(0) {
      // nop
    }
    impl(const impl&) : pos_(0) {
      // nop
    }
    void operator()(uplock& guard, const actor_vec& vec,
                    mailbox_element_ptr& ptr, execution_unit* host) {
      CAF_ASSERT(!vec.empty());
      actor selected = vec[pos_++ % vec.size()];
      guard.unlock();
      selected->enqueue(std::move(ptr), host);
    }
    std::atomic<size_t> pos_;
  };
  return impl{};
}

namespace {

void broadcast_dispatch(actor_pool::uplock&, const actor_pool::actor_vec& vec,
                        mailbox_element_ptr& ptr, execution_unit* host) {
  CAF_ASSERT(!vec.empty());
  for (size_t i = 1; i < vec.size(); ++i) {
    vec[i]->enqueue(ptr->sender, ptr->mid, ptr->msg, host);
  }
  vec.front()->enqueue(std::move(ptr), host);
}

} // namespace <anonymous>

actor_pool::policy actor_pool::broadcast() {
  return broadcast_dispatch;
}


actor_pool::policy actor_pool::random() {
  struct impl {
    impl() : rd_() {
      // nop
    }
    impl(const impl&) : rd_() {
      // nop
    }
    void operator()(uplock& guard, const actor_vec& vec,
                    mailbox_element_ptr& ptr, execution_unit* host) {
      std::uniform_int_distribution<size_t> dis(0, vec.size() - 1);
      upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
      actor selected = vec[dis(rd_)];
      unique_guard.unlock();
      selected->enqueue(std::move(ptr), host);
    }
    std::random_device rd_;
  };
  return impl{};
}

actor_pool::~actor_pool() {
  // nop
}

actor actor_pool::make(policy pol) {
  intrusive_ptr<actor_pool> ptr;
  ptr = make_counted<actor_pool>();
  ptr->policy_ = std::move(pol);
  return actor_cast<actor>(ptr);
}

actor actor_pool::make(size_t num_workers, factory fac, policy pol) {
  auto res = make(std::move(pol));
  auto ptr = static_cast<actor_pool*>(actor_cast<abstract_actor*>(res));
  auto res_addr = ptr->address();
  for (size_t i = 0; i < num_workers; ++i) {
    auto worker = fac();
    worker->attach(default_attachable::make_monitor(res_addr));
    ptr->workers_.push_back(worker);
  }
  return res;
}

void actor_pool::enqueue(const actor_addr& sender, message_id mid,
                         message content, execution_unit* eu) {
  upgrade_lock<detail::shared_spinlock> guard{workers_mtx_};
  if (filter(guard, sender, mid, content, eu)) {
    return;
  }
  auto ptr = mailbox_element::make(sender, mid, std::move(content));
  policy_(guard, workers_, ptr, eu);
}

void actor_pool::enqueue(mailbox_element_ptr what, execution_unit* eu) {
  upgrade_lock<detail::shared_spinlock> guard{workers_mtx_};
  if (filter(guard, what->sender, what->mid, what->msg, eu)) {
    return;
  }
  policy_(guard, workers_, what, eu);
}

actor_pool::actor_pool() : planned_reason_(caf::exit_reason::not_exited) {
  is_registered(true);
}

bool actor_pool::filter(upgrade_lock<detail::shared_spinlock>& guard,
                        const actor_addr& sender, message_id mid,
                        const message& msg, execution_unit* eu) {
  auto rsn = planned_reason_;
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
    // but first swap workers_ out of the critical section
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    workers_.swap(workers);
    planned_reason_ = msg.get_as<exit_msg>(0).reason;
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
    auto last = workers_.end();
    auto i = std::find(workers_.begin(), workers_.end(), dm.source);
    if (i != last) {
      workers_.erase(i);
    }
    if (workers_.empty()) {
      planned_reason_ = exit_reason::out_of_workers;
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
    workers_.push_back(worker);
    return true;
  }
  if (msg.match_elements<sys_atom, delete_atom, actor>()) {
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    auto& what = msg.get_as<actor>(2);
    auto last = workers_.end();
    auto i = std::find(workers_.begin(), last, what);
    if (i != last) {
      workers_.erase(i);
    }
    return true;
  }
  if (msg.match_elements<sys_atom, get_atom>()) {
    auto cpy = workers_;
    guard.unlock();
    actor_cast<abstract_actor*>(sender)->enqueue(invalid_actor_addr,
                                                 mid.response_id(),
                                                 make_message(std::move(cpy)),
                                                 eu);
    return true;
  }
  if (workers_.empty()) {
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
  // workers_mtx_ because abstract_actor has its own lock
  cleanup(planned_reason_);
  is_registered(false);
}

} // namespace caf
