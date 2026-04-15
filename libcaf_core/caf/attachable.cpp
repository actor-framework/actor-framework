// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/attachable.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/detail/monitor_action.hpp"
#include "caf/internal/attachable_factory.hpp"
#include "caf/internal/attachable_predicate.hpp"
#include "caf/system_messages.hpp"

namespace caf::internal {

class monitor_attachable : public attachable {
public:
  explicit monitor_attachable(actor_addr observer,
                              message_priority prio = message_priority::normal)
    : observer(std::move(observer)), priority(prio) {
    // nop
  }

  void actor_exited(abstract_actor* self, const error& rsn,
                    scheduler* sched) override {
    if (auto sptr = actor_cast<strong_actor_ptr>(observer)) {
      sptr->enqueue(make_mailbox_element(nullptr, make_message_id(priority),
                                         down_msg{self->address(), rsn}),
                    sched);
    }
  }

  bool matches(const attachable_predicate& pred) const override {
    return pred.visit(*this);
  }

  /// Holds a weak reference to the observing actor.
  actor_addr observer;

  /// Defines the priority for the message.
  message_priority priority;
};

attachable_ptr attachable_factory::make_monitor(actor_addr observer,
                                                message_priority prio) {
  using impl = internal::monitor_attachable;
  return std::make_unique<impl>(std::move(observer), prio);
}

class monitor_action_attachable : public attachable {
public:
  explicit monitor_action_attachable(detail::abstract_monitor_action_ptr ptr)
    : impl(std::move(ptr)) {
    // nop
  }

  void actor_exited(abstract_actor*, const error& reason,
                    scheduler* sched) override {
    if (!impl->set_reason(reason)) {
      return;
    }
    if (auto sptr = actor_cast<strong_actor_ptr>(impl->observer())) {
      sptr->enqueue(
        make_mailbox_element(nullptr, make_message_id(), action{impl}), sched);
    }
  }

  bool matches(const attachable_predicate& pred) const override {
    return pred.visit(*this);
  }

  /// The action to execute when the observed actor exits.
  detail::abstract_monitor_action_ptr impl;
};

attachable_ptr
attachable_factory::make_monitor(detail::abstract_monitor_action_ptr ptr) {
  using impl = internal::monitor_action_attachable;
  return std::make_unique<impl>(std::move(ptr));
}

class link_attachable : public attachable {
public:
  explicit link_attachable(actor_addr observer)
    : observer(std::move(observer)) {
    // nop
  }

  void actor_exited(abstract_actor* self, const error& reason,
                    scheduler* sched) override {
    if (auto sptr = actor_cast<strong_actor_ptr>(observer)) {
      sptr->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                         exit_msg{self->address(), reason}),
                    sched);
    }
  }

  bool matches(const attachable_predicate& pred) const override {
    return pred.visit(*this);
  }

  /// Holds a weak reference to the observing actor.
  actor_addr observer;
};

attachable_ptr attachable_factory::make_link(actor_addr observer) {
  using impl = internal::link_attachable;
  return std::make_unique<impl>(std::move(observer));
}

template <class Predicate, class Attachable>
bool matches(const Predicate&, const Attachable&) { // fallback overload
  return false;
}

bool matches(const attachable_predicate::monitored_by_state& pred,
             const monitor_attachable& what) {
  return pred.observer == what.observer.get();
}

bool matches(const attachable_predicate::monitored_with_callback_state& pred,
             const monitor_action_attachable& what) {
  return pred.impl == what.impl;
}

bool matches(const attachable_predicate::linked_to_state& pred,
             const link_attachable& what) {
  return pred.observer == what.observer.get();
}

bool matches(const attachable_predicate::observed_by_state& pred,
             const monitor_attachable& what) {
  return pred.observer == what.observer.get();
}

bool matches(const attachable_predicate::observed_by_state& pred,
             const monitor_action_attachable& what) {
  return pred.observer == what.impl->observer().get();
}

bool matches(const attachable_predicate::observed_by_state& pred,
             const link_attachable& what) {
  return pred.observer == what.observer.get();
}

bool attachable_predicate::operator()(const attachable& what) const {
  return what.matches(*this);
}

bool attachable_predicate::visit(const monitor_attachable& what) const {
  return std::visit([&what](auto& impl) { return matches(impl, what); }, impl);
}

bool attachable_predicate::visit(const monitor_action_attachable& what) const {
  return std::visit([&what](auto& impl) { return matches(impl, what); }, impl);
}

bool attachable_predicate::visit(const link_attachable& what) const {
  return std::visit([&what](auto& impl) { return matches(impl, what); }, impl);
}

} // namespace caf::internal

namespace caf {

attachable::~attachable() noexcept {
  // Avoid recursive cleanup of next pointers because this can cause a stack
  // overflow for long linked lists.
  using std::swap;
  while (next != nullptr) {
    attachable_ptr tmp;
    swap(next->next, tmp);
    swap(next, tmp);
  }
}

bool attachable::matches(const internal::attachable_predicate&) const {
  return false;
}

} // namespace caf
