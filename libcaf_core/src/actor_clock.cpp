// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_clock.hpp"

#include "caf/action.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/disposable.hpp"
#include "caf/group.hpp"
#include "caf/sec.hpp"

namespace caf {

// -- private utility ----------------------------------------------------------

namespace {

// Unlike the regular action implementation, this one is *not* thread-safe!
// Only the clock itself may access this.
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
    CAF_ASSERT(worker_ != nullptr);
    if constexpr (std::is_same_v<WorkerPtr, weak_actor_ptr>) {
      if (auto ptr = actor_cast<strong_actor_ptr>(worker_)) {
        do_run(ptr);
      } else {
        decorated_->dispose();
      }
    } else {
      do_run(worker_);
    }
    worker_ = nullptr;
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
    ptr->enqueue(nullptr, make_message_id(), make_message(action{decorated_}),
                 nullptr);
  }

  action::impl_ptr decorated_;
  WorkerPtr worker_;
};

template <class WorkerPtr>
action decorate(action f, WorkerPtr worker) {
  CAF_ASSERT(f.ptr() != nullptr);
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
  return schedule(t, decorate(std::move(f), std::move(worker)));
}

disposable actor_clock::schedule(time_point t, action f,
                                 weak_actor_ptr worker) {
  return schedule(t, decorate(std::move(f), std::move(worker)));
}

disposable actor_clock::schedule_message(time_point t,
                                         strong_actor_ptr receiver,
                                         mailbox_element_ptr content) {
  auto f = make_action(
    [rptr{std::move(receiver)}, cptr{std::move(content)}]() mutable {
      rptr->enqueue(std::move(cptr), nullptr);
    });
  schedule(t, f);
  return std::move(f).as_disposable();
}

disposable actor_clock::schedule_message(time_point t, weak_actor_ptr receiver,
                                         mailbox_element_ptr content) {
  auto f = make_action(
    [rptr{std::move(receiver)}, cptr{std::move(content)}]() mutable {
      if (auto ptr = actor_cast<strong_actor_ptr>(rptr))
        ptr->enqueue(std::move(cptr), nullptr);
    });
  schedule(t, f);
  return std::move(f).as_disposable();
}

disposable actor_clock::schedule_message(time_point t, group target,
                                         strong_actor_ptr sender,
                                         message content) {
  auto f = make_action([=]() mutable {
    if (auto dst = target->get())
      dst->enqueue(std::move(sender), make_message_id(), std::move(content),
                   nullptr);
  });
  schedule(t, f);
  return std::move(f).as_disposable();
}

} // namespace caf
