// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf::test::fixture {

// -- mailbox ------------------------------------------------------------------

class deterministic::mailbox_impl : public ref_counted,
                                    public abstract_mailbox {
public:
  mailbox_impl(deterministic* fix, scheduled_actor* owner)
    : fix_(fix), owner_(owner) {
  }

  intrusive::inbox_result push_back(mailbox_element_ptr ptr) override {
    if (closed_) {
      detail::sync_request_bouncer bouncer{close_reason_};
      bouncer(*ptr);
      return intrusive::inbox_result::queue_closed;
    }
    using event_t = deterministic::scheduling_event;
    auto unblocked = fix_->mail_count(owner_) == 0;
    auto event = std::make_unique<event_t>(owner_, std::move(ptr));
    fix_->events_.push_back(std::move(event));
    return unblocked ? intrusive::inbox_result::unblocked_reader
                     : intrusive::inbox_result::success;
  }

  void push_front(mailbox_element_ptr ptr) override {
    using event_t = deterministic::scheduling_event;
    auto event = std::make_unique<event_t>(owner_, std::move(ptr));
    fix_->events_.emplace_front(std::move(event));
  }

  mailbox_element_ptr pop_front() override {
    return fix_->pop_msg_impl(owner_);
  }

  bool closed() const noexcept override {
    return closed_;
  }

  bool blocked() const noexcept override {
    return blocked_;
  }

  bool try_block() override {
    blocked_ = true;
    return true;
  }

  bool try_unblock() override {
    if (!blocked_)
      return false;
    blocked_ = false;
    return true;
  }

  size_t close(const error& reason) override {
    closed_ = true;
    close_reason_ = reason;
    auto result = size_t{0};
    auto envelope = fix_->pop_msg_impl(owner_);
    while (envelope != nullptr) {
      detail::sync_request_bouncer bouncer{reason};
      ++result;
      envelope = fix_->pop_msg_impl(owner_);
    }
    return result;
  }

  size_t size() override {
    return fix_->mail_count(owner_);
  }

  void ref_mailbox() noexcept override {
    ref();
  }

  void deref_mailbox() noexcept override {
    deref();
  }

  mailbox_element* peek(message_id) override {
    // Note: this function only exists for backwards compatibility with the old
    // unit testing framework. It is not used by the new test runner and thus
    // not implemented.
    CAF_RAISE_ERROR(std::logic_error, "peek not supported by this mailbox");
  }

private:
  bool blocked_ = false;
  bool closed_ = false;
  error close_reason_;
  deterministic* fix_;
  scheduled_actor* owner_;
};

class deterministic::mailbox_factory_impl : public detail::mailbox_factory {
public:
  explicit mailbox_factory_impl(deterministic* fix) : fix_(fix) {
    // nop
  }

  abstract_mailbox* make(scheduled_actor* owner) override {
    return new deterministic::mailbox_impl(fix_, owner);
  }

  abstract_mailbox* make(blocking_actor*) override {
    return nullptr;
  }

private:
  deterministic* fix_;
};

// -- scheduler ----------------------------------------------------------------

class deterministic::scheduler_impl : public scheduler::abstract_coordinator {
public:
  using super = caf::scheduler::abstract_coordinator;

  scheduler_impl(actor_system& sys, deterministic* fix)
    : super(sys), fix_(fix) {
    // nop
  }

  bool detaches_utility_actors() const override {
    return false;
  }

  detail::test_actor_clock& clock() noexcept override {
    return clock_;
  }

protected:
  void start() override {
    // nop
  }

  void stop() override {
    // nop
  }

  void enqueue(resumable* ptr) override {
    using event_t = deterministic::scheduling_event;
    using subtype_t = resumable::subtype_t;
    switch (ptr->subtype()) {
      case subtype_t::scheduled_actor:
      case subtype_t::io_actor: {
        // Actors put their messages into events_ directly. However, we do run
        // them right away if they aren't initialized yet.
        auto dptr = static_cast<scheduled_actor*>(ptr);
        if (!dptr->initialized())
          dptr->resume(system_.dummy_execution_unit(), 0);
        break;
      }
      default:
        fix_->events_.push_back(std::make_unique<event_t>(ptr, nullptr));
    }
    // Before calling this function, CAF *always* bumps the reference count.
    // Hence, we need to release one reference count here.
    intrusive_ptr_release(ptr);
  }

private:
  /// The fixture this scheduler belongs to.
  deterministic* fix_;

  /// Allows users to fake time at will.
  detail::test_actor_clock clock_;
};

// -- config -------------------------------------------------------------------

deterministic::config::config(deterministic* fix) {
  factory_ = std::make_unique<mailbox_factory_impl>(fix);
  module_factories.push_back([fix](actor_system& sys) -> actor_system::module* {
    return new scheduler_impl(sys, fix);
  });
}

deterministic::config::~config() {
  // nop
}

detail::mailbox_factory* deterministic::config::mailbox_factory() {
  return factory_.get();
}

// -- abstract_message_predicate -----------------------------------------------

deterministic::abstract_message_predicate::~abstract_message_predicate() {
  // nop
}

// -- fixture ------------------------------------------------------------------

deterministic::deterministic() : cfg(this), sys(cfg) {
  // nop
}

deterministic::~deterministic() {
  // Note: we need clean up all remaining messages manually. This in turn may
  //       clean up actors as unreachable if the test did not consume all
  //       messages. Otherwise, the destructor of `sys` will wait for all
  //       actors, potentially waiting forever. However, we cannot just call
  //       `events_.clear()`, because that would potentially cause an actor to
  //       become unreachable and close its mailbox. This would call
  //       `pop_msg_impl` in turn, which then tries to alter the list while
  //       we're clearing it.
  while (!events_.empty()) {
    std::list<std::unique_ptr<scheduling_event>> tmp;
    tmp.splice(tmp.end(), events_);
    // Here, tmp will be destroyed and cleanup code of actors might send more
    // messages. Hence the loop.
  }
}

bool deterministic::prepone_event_impl(const strong_actor_ptr& receiver) {
  actor_predicate any_sender{std::ignore};
  message_predicate any_payload{std::ignore};
  return prepone_event_impl(receiver, any_sender, any_payload);
}

bool deterministic::prepone_event_impl(
  const strong_actor_ptr& receiver, actor_predicate& sender_pred,
  abstract_message_predicate& payload_pred) {
  if (events_.empty() || !receiver)
    return false;
  auto first = events_.begin();
  auto last = events_.end();
  auto i = std::find_if(first, last, [&](const auto& event) {
    auto self = actor_cast<abstract_actor*>(receiver);
    return event->target == static_cast<scheduled_actor*>(self)
           && sender_pred(event->item->sender)
           && payload_pred(event->item->payload);
  });
  if (i == last)
    return false;
  if (i != first) {
    auto ptr = std::move(*i);
    events_.erase(i);
    events_.insert(events_.begin(), std::move(ptr));
  }
  return true;
}

deterministic::scheduling_event*
deterministic::find_event_impl(const strong_actor_ptr& receiver) {
  if (events_.empty() || !receiver)
    return nullptr;
  auto last = events_.end();
  auto i = std::find_if(events_.begin(), last, [&](const auto& event) {
    auto raw_ptr = actor_cast<abstract_actor*>(receiver);
    return event->target == static_cast<scheduled_actor*>(raw_ptr);
  });
  if (i != last)
    return i->get();
  return nullptr;
}

mailbox_element_ptr deterministic::pop_msg_impl(scheduled_actor* receiver) {
  auto pred = [&](const auto& event) { return event->target == receiver; };
  auto first = events_.begin();
  auto last = events_.end();
  auto i = std::find_if(first, last, pred);
  if (i == last)
    return nullptr;
  auto result = std::move((*i)->item);
  events_.erase(i);
  return result;
}

size_t deterministic::mail_count(scheduled_actor* receiver) {
  if (receiver == nullptr)
    return 0;
  auto pred = [&](const auto& event) { return event->target == receiver; };
  return std::count_if(events_.begin(), events_.end(), pred);
}

size_t deterministic::mail_count(const strong_actor_ptr& receiver) {
  auto raw_ptr = actor_cast<abstract_actor*>(receiver);
  return mail_count(dynamic_cast<scheduled_actor*>(raw_ptr));
}

bool deterministic::dispatch_message() {
  if (events_.empty())
    return false;
  if (events_.front()->item == nullptr) {
    // Regular resumable.
    auto ev = std::move(events_.front());
    events_.pop_front();
    ev->target->resume(sys.dummy_execution_unit(), 0);
    return true;
  }
  // Actor: we simply resume the next actor and it will pick up its message.
  auto next = events_.front()->target;
  next->resume(sys.dummy_execution_unit(), 1);
  return true;
}

size_t deterministic::dispatch_messages() {
  size_t result = 0;
  while (dispatch_message())
    ++result;
  return result;
}

} // namespace caf::test
