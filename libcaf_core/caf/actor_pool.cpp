// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_pool.hpp"

#include "caf/anon_mail.hpp"
#include "caf/default_attachable.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/mailbox_element.hpp"

#include <atomic>
#include <random>

namespace caf {

actor_pool::policy actor_pool::round_robin() {
  struct impl {
    impl() : pos_(0) {
      // nop
    }
    impl(const impl&) : pos_(0) {
      // nop
    }
    void operator()(actor_system&, guard_type& guard, const actor_vec& vec,
                    mailbox_element_ptr& ptr, scheduler* sched) {
      CAF_ASSERT(!vec.empty());
      actor selected = vec[pos_++ % vec.size()];
      guard.unlock();
      selected->enqueue(std::move(ptr), sched);
    }
    std::atomic<size_t> pos_;
  };
  return impl{};
}

namespace {

void broadcast_dispatch(actor_system&, actor_pool::guard_type&,
                        const actor_pool::actor_vec& vec,
                        mailbox_element_ptr& ptr, scheduler* sched) {
  CAF_ASSERT(!vec.empty());
  auto msg = ptr->payload;
  for (auto& worker : vec)
    worker->enqueue(make_mailbox_element(ptr->sender, ptr->mid, msg), sched);
}

} // namespace

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
    void operator()(actor_system&, guard_type& guard, const actor_vec& vec,
                    mailbox_element_ptr& ptr, scheduler* sched) {
      auto selected
        = vec[dis_(rd_, decltype(dis_)::param_type(0, vec.size() - 1))];
      guard.unlock();
      selected->enqueue(std::move(ptr), sched);
    }
    std::random_device rd_;
    std::uniform_int_distribution<size_t> dis_;
  };
  return impl{};
}

actor_pool::~actor_pool() {
  // nop
}

actor actor_pool::make(actor_system& sys, policy pol) {
  actor_config cfg{&sys.scheduler()};
  auto res = make_actor<actor_pool, actor>(sys.next_actor_id(), sys.node(),
                                           &sys, cfg);
  auto ptr = static_cast<actor_pool*>(actor_cast<abstract_actor*>(res));
  ptr->policy_ = std::move(pol);
  return res;
}

actor actor_pool::make(actor_system& sys, size_t num_workers,
                       const factory& fac, policy pol) {
  auto res = make(sys, std::move(pol));
  auto ptr = static_cast<actor_pool*>(actor_cast<abstract_actor*>(res));
  auto res_addr = ptr->address();
  for (size_t i = 0; i < num_workers; ++i) {
    auto worker = fac();
    worker->attach(
      default_attachable::make_monitor(worker.address(), res_addr));
    ptr->workers_.push_back(std::move(worker));
  }
  return res;
}

bool actor_pool::enqueue(mailbox_element_ptr what, scheduler* sched) {
  guard_type guard{workers_mtx_};
  if (filter(guard, what->sender, what->mid, what->payload, sched))
    return false;
  policy_(home_system(), guard, workers_, what, sched);
  return true;
}

actor_pool::actor_pool(actor_config& cfg)
  : abstract_actor(cfg), planned_reason_(exit_reason::normal) {
  register_at_system();
}

const char* actor_pool::name() const {
  return "caf.actor-pool";
}

void actor_pool::on_cleanup([[maybe_unused]] const error& reason) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_LOG_TERMINATE_EVENT(this, reason);
}

bool actor_pool::filter(guard_type& guard, const strong_actor_ptr& sender,
                        message_id mid, message& content, scheduler* sched) {
  auto lg = log::core::trace("mid = {}, content = {}", mid, content);
  if (auto view = make_const_typed_message_view<exit_msg>(content)) {
    // acquire second mutex as well
    std::vector<actor> workers;
    auto reason = get<0>(view).reason;
    if (cleanup(std::move(reason), sched)) {
      // send exit messages *always* to all workers and clear vector afterwards
      // but first swap workers_ out of the critical section
      workers_.swap(workers);
      guard.unlock();
      for (auto& w : workers)
        anon_mail(content).send(w);
      unregister_from_system();
    }
    return true;
  }
  if (auto view = make_const_typed_message_view<down_msg>(content)) {
    // remove failed worker from pool
    const auto& dm = get<0>(view);
    auto last = workers_.end();
    auto i = std::find(workers_.begin(), workers_.end(), dm.source);
    CAF_LOG_DEBUG_IF(i == last, "received down message for an unknown worker");
    if (i != last)
      workers_.erase(i);
    if (workers_.empty()) {
      planned_reason_ = exit_reason::out_of_workers;
      guard.unlock();
      quit(sched);
    }
    return true;
  }
  if (auto view
      = make_const_typed_message_view<sys_atom, put_atom, actor>(content)) {
    const auto& worker = get<2>(view);
    worker->attach(
      default_attachable::make_monitor(worker.address(), address()));
    workers_.push_back(worker);
    return true;
  }
  if (auto view
      = make_const_typed_message_view<sys_atom, delete_atom, actor>(content)) {
    auto& what = get<2>(view);
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
    sender->enqueue(
      make_mailbox_element(nullptr, mid.response_id(), std::move(cpy)), sched);
    return true;
  }
  if (workers_.empty()) {
    guard.unlock();
    if (mid.is_request() && sender != nullptr) {
      // Tell client we have ignored this request message by sending and empty
      // message back.
      sender->enqueue(
        make_mailbox_element(nullptr, mid.response_id(), message{}), sched);
    }
    return true;
  }
  return false;
}

void actor_pool::quit(scheduler* sched) {
  // we can safely run our cleanup code here without holding
  // workers_mtx_ because abstract_actor has its own lock
  if (cleanup(planned_reason_, sched))
    unregister_from_system();
}

void actor_pool::force_close_mailbox() {
  // nop
}

} // namespace caf
