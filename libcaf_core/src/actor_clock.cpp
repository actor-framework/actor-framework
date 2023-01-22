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

  using transition = action::transition;

  action_decorator(action::impl_ptr decorated, WorkerPtr worker,
                   actor_clock::stall_policy policy)
    : decorated_(std::move(decorated)),
      worker_(std::move(worker)),
      policy_(policy) {
    CAF_ASSERT(decorated_ != nullptr);
    CAF_ASSERT(worker_ != nullptr);
  }

  void dispose() override {
    if (decorated_) {
      decorated_->dispose();
      decorated_ = nullptr;
    }
    if (worker_)
      worker_ = nullptr;
  }

  bool disposed() const noexcept override {
    return decorated_ ? decorated_->disposed() : true;
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  transition reschedule() override {
    // Always succeeds since we implicitly reschedule in do_run.
    return transition::success;
  }

  transition run() override {
    CAF_ASSERT(decorated_ != nullptr);
    CAF_ASSERT(worker_ != nullptr);
    if constexpr (std::is_same_v<WorkerPtr, weak_actor_ptr>) {
      if (auto ptr = actor_cast<strong_actor_ptr>(worker_)) {
        return do_run(ptr);
      } else {
        dispose();
        return transition::disposed;
      }
    } else {
      return do_run(worker_);
    }
  }

  state current_state() const noexcept override {
    return decorated_ ? decorated_->current_state() : action::state::disposed;
  }

  friend void intrusive_ptr_add_ref(const action_decorator* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const action_decorator* ptr) noexcept {
    ptr->deref();
  }

private:
  transition do_run(strong_actor_ptr& ptr) {
    switch (decorated_->reschedule()) {
      case transition::disposed:
        decorated_ = nullptr;
        worker_ = nullptr;
        return transition::disposed;
      case transition::success:
        if (ptr->enqueue(nullptr, make_message_id(),
                         make_message(action{decorated_}), nullptr)) {
          return transition::success;
        } else {
          dispose();
          return transition::disposed;
        }
      default:
        if (policy_ == actor_clock::stall_policy::fail) {
          ptr->enqueue(nullptr, make_message_id(),
                       make_message(make_error(sec::action_reschedule_failed)),
                       nullptr);
          dispose();
          return transition::failure;
        } else {
          return transition::success;
        }
    }
  }

  action::impl_ptr decorated_;
  WorkerPtr worker_;
  actor_clock::stall_policy policy_;
};

template <class WorkerPtr>
action decorate(action f, WorkerPtr worker, actor_clock::stall_policy policy) {
  CAF_ASSERT(f.ptr() != nullptr);
  using impl_t = action_decorator<WorkerPtr>;
  auto ptr = make_counted<impl_t>(std::move(f).as_intrusive_ptr(),
                                  std::move(worker), policy);
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
  return schedule_periodically(time_point{duration_type{0}}, std::move(f),
                               duration_type{0});
}

disposable actor_clock::schedule(time_point t, action f) {
  return schedule_periodically(t, std::move(f), duration_type{0});
}

disposable actor_clock::schedule(time_point t, action f,
                                 strong_actor_ptr worker) {
  return schedule_periodically(t, std::move(f), std::move(worker),
                               duration_type{0}, stall_policy::skip);
}

disposable actor_clock::schedule_periodically(time_point first_run, action f,
                                              strong_actor_ptr worker,
                                              duration_type period,
                                              stall_policy policy) {
  auto res = f.as_disposable();
  auto g = decorate(std::move(f), std::move(worker), policy);
  schedule_periodically(first_run, std::move(g), period);
  return res;
}

disposable actor_clock::schedule(time_point t, action f,
                                 weak_actor_ptr worker) {
  return schedule_periodically(t, std::move(f), std::move(worker),
                               duration_type{0}, stall_policy::skip);
}

disposable actor_clock::schedule_periodically(time_point first_run, action f,
                                              weak_actor_ptr worker,
                                              duration_type period,
                                              stall_policy policy) {
  auto res = f.as_disposable();
  auto g = decorate(std::move(f), std::move(worker), policy);
  schedule_periodically(first_run, std::move(g), period);
  return res;
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
