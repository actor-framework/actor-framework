// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_clock.hpp"

#include "caf/action.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/disposable.hpp"
#include "caf/sec.hpp"

namespace caf {

// -- private utility ----------------------------------------------------------

namespace {

/// Decorates an action by adding a worker to it that will run the action.
template <class WorkerPtr>
class action_decorator : public ref_counted, public action::impl {
public:
  using state = action::state;

  action_decorator(action::impl_ptr decorated, WorkerPtr worker)
    : decorated_(std::move(decorated)), worker_(std::move(worker)) {
    CAF_ASSERT(decorated_ != nullptr);
    CAF_ASSERT(worker_ != nullptr);
  }

  void dispose() override {
    decorated_->dispose();
    std::unique_lock guard(mtx_);
    worker_ = nullptr;
  }

  bool disposed() const noexcept override {
    return decorated_->disposed();
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  void run() override {
    CAF_ASSERT(decorated_ != nullptr);
    WorkerPtr tmp;
    {
      std::unique_lock guard(mtx_);
      tmp.swap(worker_);
    }
    if constexpr (std::is_same_v<WorkerPtr, weak_actor_ptr>) {
      if (auto ptr = actor_cast<strong_actor_ptr>(tmp)) {
        do_run(ptr);
      } else {
        decorated_->dispose();
      }
    } else {
      if (tmp)
        do_run(tmp);
    }
  }

  state current_state() const noexcept override {
    return decorated_->current_state();
  }

  friend void intrusive_ptr_add_ref(const action_decorator* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const action_decorator* ptr) noexcept {
    ptr->deref();
  }

private:
  void do_run(strong_actor_ptr& ptr) {
    if (!decorated_->disposed())
      ptr->enqueue(nullptr, make_message_id(), make_message(action{decorated_}),
                   nullptr);
  }

  std::mutex mtx_;
  action::impl_ptr decorated_;
  WorkerPtr worker_;
};

template <class WorkerPtr>
action decorate(action f, WorkerPtr worker) {
  CAF_ASSERT(f.ptr() != nullptr);
  CAF_ASSERT(worker != nullptr);
  using impl_t = action_decorator<WorkerPtr>;
  auto ptr = make_counted<impl_t>(std::move(f).as_intrusive_ptr(),
                                  std::move(worker));
  return action{std::move(ptr)};
}

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

actor_clock::~actor_clock() {
  // nop
}

// -- scheduling ---------------------------------------------------------------

actor_clock::time_point actor_clock::now() const noexcept {
  return clock_type::now();
}

disposable actor_clock::schedule(action f) {
  return schedule(time_point{duration_type{0}}, std::move(f));
}

disposable actor_clock::schedule(time_point t, action f,
                                 strong_actor_ptr worker) {
  auto decorated = decorate(std::move(f), std::move(worker));
  schedule(t, decorated);
  return std::move(decorated).as_disposable();
}

disposable actor_clock::schedule(time_point t, action f,
                                 weak_actor_ptr worker) {
  auto decorated = decorate(std::move(f), std::move(worker));
  schedule(t, decorated);
  return std::move(decorated).as_disposable();
}

disposable actor_clock::schedule_message(time_point t,
                                         strong_actor_ptr receiver,
                                         mailbox_element_ptr content) {
  auto f = make_action(
    [rptr{std::move(receiver)}, cptr{std::move(content)}]() mutable {
      rptr->enqueue(std::move(cptr), nullptr);
    });
  return schedule(t, std::move(f));
}

disposable actor_clock::schedule_message(time_point t, weak_actor_ptr receiver,
                                         mailbox_element_ptr content) {
  auto f = make_action(
    [rptr{std::move(receiver)}, cptr{std::move(content)}]() mutable {
      if (auto ptr = actor_cast<strong_actor_ptr>(rptr))
        ptr->enqueue(std::move(cptr), nullptr);
    });
  return schedule(t, std::move(f));
}

} // namespace caf
