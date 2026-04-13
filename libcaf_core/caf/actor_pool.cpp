// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_pool.hpp"

#include "caf/actor_cast.hpp"
#include "caf/anon_mail.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/internal/attachable_factory.hpp"
#include "caf/internal/attachable_predicate.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include <atomic>
#include <random>

CAF_PUSH_DEPRECATED_WARNING

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
  actor_config cfg{no_spawn_options, &sys.scheduler()};
  auto res = make_actor<actor_pool, actor>(sys.next_actor_id(), sys.node(),
                                           &sys, cfg);
  auto ptr = actor_cast<actor_pool*>(res);
  ptr->setf(abstract_actor::is_registered_flag);
  sys.inc_running_actors_count(ptr->id());
  ptr->policy_ = std::move(pol);
  return res;
}

actor actor_pool::make(actor_system& sys, size_t num_workers,
                       const factory& fac, policy pol) {
  using internal::attachable_factory;
  auto res = make(sys, std::move(pol));
  auto* self = actor_cast<actor_pool*>(res);
  for (size_t i = 0; i < num_workers; ++i) {
    auto worker = fac();
    auto* wptr = actor_cast<abstract_actor*>(worker);
    self->add_monitor(wptr, attachable_factory::make_monitor(self->address()));
    self->workers_.push_back(std::move(worker));
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
  // nop
}

const char* actor_pool::name() const {
  return "caf.actor-pool";
}

void actor_pool::on_cleanup([[maybe_unused]] const error& reason) {
  detail::current_actor_guard ctx_guard{this};
  CAF_LOG_TERMINATE_EVENT(this, reason);
}

bool actor_pool::filter(guard_type& guard, const strong_actor_ptr& sender,
                        message_id mid, message& content, scheduler* sched) {
  auto lg = log::core::trace("mid = {}, content = {}", mid, content);
  if (const_typed_message_view<exit_msg> view{content}) {
    const auto& em = get<0>(view);
    unlink_from(em.source);
    auto reason = em.reason;
    if (cleanup(std::move(reason), sched)) {
      std::vector<actor> workers;
      // send exit messages *always* to all workers and clear vector afterwards
      // but first swap workers_ out of the critical section
      workers_.swap(workers);
      guard.unlock();
      for (auto& w : workers)
        anon_mail(content).send(w);
    }
    return true;
  }
  if (const_typed_message_view<down_msg> view{content}) {
    // remove failed worker from pool
    const auto& dm = get<0>(view);
    clear_incoming_edges(dm.source);
    auto i = std::find(workers_.begin(), workers_.end(), dm.source);
    if (i == workers_.end()) {
      log::core::debug("received down message for an unknown worker");
    } else {
      workers_.erase(i);
    }
    if (workers_.empty()) {
      planned_reason_ = exit_reason::out_of_workers;
      guard.unlock();
      quit(sched);
    }
    return true;
  }
  if (const_typed_message_view<sys_atom, put_atom, actor> view{content}) {
    using factory = internal::attachable_factory;
    const auto& worker = get<2>(view);
    auto* wptr = actor_cast<abstract_actor*>(worker);
    add_monitor(wptr, factory::make_monitor(address()));
    workers_.push_back(worker);
    return true;
  }
  if (const_typed_message_view<sys_atom, delete_atom, actor> view{content}) {
    const auto& what = get<2>(view);
    auto i = std::find(workers_.begin(), workers_.end(), what);
    if (i != workers_.end()) {
      del_monitor(actor_cast<abstract_actor*>(what),
                  internal::attachable_predicate::monitored_by(ctrl()));
      workers_.erase(i);
    }
    return true;
  }
  if (content.match_elements<sys_atom, delete_atom>()) {
    for (const auto& worker : workers_) {
      del_monitor(actor_cast<abstract_actor*>(worker),
                  internal::attachable_predicate::monitored_by(ctrl()));
    }
    workers_.clear();
    return true;
  }
  if (content.match_elements<sys_atom, get_atom>()) {
    auto copy = workers_;
    guard.unlock();
    sender->enqueue(
      make_mailbox_element(nullptr, mid.response_id(), std::move(copy)), sched);
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
  cleanup(planned_reason_, sched);
}

bool actor_pool::try_force_close_mailbox() {
  return true;
}

} // namespace caf

CAF_POP_WARNINGS
