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
    void operator()(actor_system&, uplock& guard, const actor_vec& vec,
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

void broadcast_dispatch(actor_system&, actor_pool::uplock&,
                        const actor_pool::actor_vec& vec,
                        mailbox_element_ptr& ptr, execution_unit* host) {
  CAF_ASSERT(!vec.empty());
  auto msg = ptr->move_content_to_message();
  for (auto& worker : vec)
    worker->enqueue(ptr->sender, ptr->mid, msg, host);
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
    void operator()(actor_system&, uplock& guard, const actor_vec& vec,
                    mailbox_element_ptr& ptr, execution_unit* host) {
      upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
      auto selected =
          vec[dis_(rd_, decltype(dis_)::param_type(0, vec.size() - 1))];
      unique_guard.unlock();
      selected->enqueue(std::move(ptr), host);
    }
    std::random_device rd_;
    std::uniform_int_distribution<size_t> dis_;
  };
  return impl{};
}

actor_pool::~actor_pool() {
  // nop
}

actor actor_pool::make(execution_unit* eu, policy pol) {
  CAF_ASSERT(eu);
  auto& sys = eu->system();
  actor_config cfg{eu};
  auto res = make_actor<actor_pool, actor>(sys.next_actor_id(), sys.node(),
                                           &sys, cfg);
  auto ptr = static_cast<actor_pool*>(actor_cast<abstract_actor*>(res));
  ptr->policy_ = std::move(pol);
  return res;
}

actor actor_pool::make(execution_unit* eu, size_t num_workers,
                       const factory& fac, policy pol) {
  auto res = make(eu, std::move(pol));
  auto ptr = static_cast<actor_pool*>(actor_cast<abstract_actor*>(res));
  auto res_addr = ptr->address();
  for (size_t i = 0; i < num_workers; ++i) {
    auto worker = fac();
    worker->attach(default_attachable::make_monitor(worker.address(), res_addr));
    ptr->workers_.push_back(std::move(worker));
  }
  return res;
}

void actor_pool::enqueue(mailbox_element_ptr what, execution_unit* eu) {
  upgrade_lock<detail::shared_spinlock> guard{workers_mtx_};
  if (filter(guard, what->sender, what->mid, *what, eu))
    return;
  policy_(home_system(), guard, workers_, what, eu);
}

actor_pool::actor_pool(actor_config& cfg)
    : monitorable_actor(cfg),
      planned_reason_(exit_reason::normal) {
  register_at_system();
}

void actor_pool::on_destroy() {
  CAF_PUSH_AID_FROM_PTR(this);
  if (!getf(is_cleaned_up_flag)) {
    cleanup(exit_reason::unreachable, nullptr);
    monitorable_actor::on_destroy();
    unregister_from_system();
  }
}

void actor_pool::on_cleanup(const error& reason) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_IGNORE_UNUSED(reason);
  CAF_LOG_TERMINATE_EVENT(this, reason);
}

bool actor_pool::filter(upgrade_lock<detail::shared_spinlock>& guard,
                        const strong_actor_ptr& sender, message_id mid,
                        message_view& mv, execution_unit* eu) {
  auto& content = mv.content();
  CAF_LOG_TRACE(CAF_ARG(mid) << CAF_ARG(content));
  if (content.match_elements<exit_msg>()) {
    // acquire second mutex as well
    std::vector<actor> workers;
    auto em = content.get_as<exit_msg>(0).reason;
    if (cleanup(std::move(em), eu)) {
      auto tmp = mv.move_content_to_message();
      // send exit messages *always* to all workers and clear vector afterwards
      // but first swap workers_ out of the critical section
      upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
      workers_.swap(workers);
      unique_guard.unlock();
      for (auto& w : workers)
        anon_send(w, tmp);
      unregister_from_system();
    }
    return true;
  }
  if (content.match_elements<down_msg>()) {
    // remove failed worker from pool
    auto& dm = content.get_as<down_msg>(0);
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    auto last = workers_.end();
    auto i = std::find(workers_.begin(), workers_.end(), dm.source);
    CAF_LOG_DEBUG_IF(i == last, "received down message for an unknown worker");
    if (i != last)
      workers_.erase(i);
    if (workers_.empty()) {
      planned_reason_ = exit_reason::out_of_workers;
      unique_guard.unlock();
      quit(eu);
    }
    return true;
  }
  if (content.match_elements<sys_atom, put_atom, actor>()) {
    auto& worker = content.get_as<actor>(2);
    worker->attach(default_attachable::make_monitor(worker.address(),
                                                    address()));
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    workers_.push_back(worker);
    return true;
  }
  if (content.match_elements<sys_atom, delete_atom, actor>()) {
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    auto& what = content.get_as<actor>(2);
    auto last = workers_.end();
    auto i = std::find(workers_.begin(), last, what);
    if (i != last) {
      default_attachable::observe_token tk{address(),
                                           default_attachable::monitor};
      what->detach(tk);
      workers_.erase(i);
    }
    return true;
  }
  if (content.match_elements<sys_atom, delete_atom>()) {
    upgrade_to_unique_lock<detail::shared_spinlock> unique_guard{guard};
    for (auto& worker : workers_) {
      default_attachable::observe_token tk{address(),
                                           default_attachable::monitor};
      worker->detach(tk);
    }
    workers_.clear();
    return true;
  }
  if (content.match_elements<sys_atom, get_atom>()) {
    auto cpy = workers_;
    guard.unlock();
    sender->enqueue(nullptr, mid.response_id(),
                    make_message(std::move(cpy)), eu);
    return true;
  }
  if (workers_.empty()) {
    guard.unlock();
    if (sender && mid.valid()) {
      // tell client we have ignored this sync message by sending
      // and empty message back
      sender->enqueue(nullptr, mid.response_id(), message{}, eu);
    }
    return true;
  }
  return false;
}

void actor_pool::quit(execution_unit* host) {
  // we can safely run our cleanup code here without holding
  // workers_mtx_ because abstract_actor has its own lock
  if (cleanup(planned_reason_, host))
    unregister_from_system();
}

} // namespace caf
