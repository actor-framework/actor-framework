// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/blocking_actor.hpp"

#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/anon_mail.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/log/system.hpp"
#include "caf/logger.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/telemetry/timer.hpp"

#include <utility>

namespace caf {

blocking_actor::receive_cond::~receive_cond() {
  // nop
}

bool blocking_actor::receive_cond::pre() {
  return true;
}

bool blocking_actor::receive_cond::post() {
  return true;
}

blocking_actor::accept_one_cond::~accept_one_cond() {
  // nop
}

bool blocking_actor::accept_one_cond::post() {
  return false;
}

blocking_actor::blocking_actor(actor_config& cfg)
  : super(cfg.add_flag(local_actor::is_blocking_flag)) {
  // nop
}

blocking_actor::~blocking_actor() {
  // avoid weak-vtables warning
}

bool blocking_actor::enqueue(mailbox_element_ptr ptr, scheduler*) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(getf(is_blocking_flag));
  auto lg = log::core::trace("ptr = {}", *ptr);
  CAF_LOG_SEND_EVENT(ptr);
  auto mid = ptr->mid;
  auto src = ptr->sender;
  auto collects_metrics = getf(abstract_actor::collects_metrics_flag);
  if (collects_metrics) {
    ptr->set_enqueue_time();
    metrics_.mailbox_size->inc();
  }
  // returns false if mailbox has been closed
  switch (mailbox().push_back(std::move(ptr))) {
    case intrusive::inbox_result::queue_closed: {
      CAF_LOG_REJECT_EVENT();
      home_system().base_metrics().rejected_messages->inc();
      if (collects_metrics)
        metrics_.mailbox_size->dec();
      if (mid.is_request()) {
        detail::sync_request_bouncer srb{exit_reason()};
        srb(src, mid);
      }
      return false;
    }
    case intrusive::inbox_result::unblocked_reader: {
      CAF_LOG_ACCEPT_EVENT(true);
      std::unique_lock guard{mtx_};
      cv_.notify_one();
      return true;
    }
    default:
      CAF_LOG_ACCEPT_EVENT(false);
      return true;
  }
}

mailbox_element* blocking_actor::peek_at_next_mailbox_element() {
  return mailbox_.peek(make_message_id());
}

const char* blocking_actor::name() const {
  return "user.blocking-actor";
}

namespace {

// Runner for passing a blocking actor to a private_thread. We don't actually
// need a reference count here, because the private thread calls
// intrusive_ptr_release_impl exactly once after running this function object.
class blocking_actor_runner : public resumable {
public:
  explicit blocking_actor_runner(blocking_actor* self,
                                 detail::private_thread* thread, bool hidden)
    : self_(self), thread_(thread), hidden_(hidden) {
    intrusive_ptr_add_ref(self->ctrl());
  }

  resumable::subtype_t subtype() const noexcept final {
    return resumable::function_object;
  }

  resumable::resume_result resume(scheduler* ctx, size_t) override {
    CAF_PUSH_AID_FROM_PTR(self_);
    self_->context(ctx);
    self_->initialize();
    error rsn;
#ifdef CAF_ENABLE_EXCEPTIONS
    try {
      self_->act();
      rsn = self_->fail_state();
    } catch (...) {
      auto ptr = std::current_exception();
      rsn = scheduled_actor::default_exception_handler(self_, ptr);
    }
#else
    self_->act();
    rsn = self_->fail_state();
#endif
    self_->cleanup(std::move(rsn), ctx);
    intrusive_ptr_release(self_->ctrl());
    auto& sys = self_->system();
    sys.release_private_thread(thread_);
    if (!hidden_) {
      [[maybe_unused]] auto count = sys.registry().dec_running();
      log::system::debug("actor {} decreased running count to {}", self_->id(),
                         count);
    }
    return resumable::done;
  }

  void ref_resumable() const noexcept final {
    // nop
  }

  void deref_resumable() const noexcept final {
    delete this;
  }

private:
  blocking_actor* self_;
  detail::private_thread* thread_;
  bool hidden_;
};

} // namespace

void blocking_actor::launch(scheduler*, bool, bool hide) {
  CAF_PUSH_AID_FROM_PTR(this);
  auto lg = log::core::trace("hide = {}", hide);
  CAF_ASSERT(getf(is_blocking_flag));
  // Try to acquire a thread before incrementing the running count, since this
  // may throw.
  auto& sys = home_system();
  auto thread = sys.acquire_private_thread();
  // Note: must *not* call register_at_system() to stop actor cleanup from
  // decrementing the count before releasing the thread.
  if (!hide) {
    [[maybe_unused]] auto count = sys.registry().inc_running();
    log::system::debug("actor {} increased running count to {}", id(), count);
  }
  thread->resume(new blocking_actor_runner(this, thread, hide));
}

blocking_actor::receive_while_helper
blocking_actor::receive_while(std::function<bool()> stmt) {
  return {this, std::move(stmt)};
}

blocking_actor::receive_while_helper
blocking_actor::receive_while(const bool& ref) {
  return receive_while([&] { return ref; });
}

void blocking_actor::await_all_other_actors_done() {
  system().registry().await_running_count_equal(getf(is_registered_flag) ? 1
                                                                         : 0);
}

void blocking_actor::act() {
  auto lg = log::core::trace("");
  if (initial_behavior_fac_)
    initial_behavior_fac_(this);
}

void blocking_actor::fail_state(error err) {
  fail_state_ = std::move(err);
}

void blocking_actor::receive_impl(receive_cond& rcc, message_id mid,
                                  detail::blocking_behavior& bhvr) {
  auto lg = log::core::trace("mid = {}", mid);
  unstash();
  // Convenience function for trying to consume a message.
  auto consume = [this, mid, &bhvr] {
    detail::default_invoke_result_visitor<blocking_actor> visitor{this};
    if (bhvr.nested(visitor, current_element_->content()))
      return true;
    auto sres = bhvr.fallback(current_element_->payload);
    auto f = detail::make_overload(
      [this, mid, &bhvr](skip_t&) {
        // Response handlers must get re-invoked with an error when
        // receiving an unexpected message.
        if (mid.is_response()) {
          auto& x = *current_element_;
          auto err = make_error(sec::unexpected_response, std::move(x.payload));
          mailbox_element tmp{std::move(x.sender), x.mid,
                              make_message(std::move(err))};
          current_element_ = &tmp;
          bhvr.nested(tmp.content());
          return true;
        }
        return false;
      },
      [&visitor](auto& res) {
        visitor(res);
        return true;
      });
    return visit(f, sres);
  };
  // Check pre-condition once before entering the message consumption loop. The
  // consumer performs any future check on pre and post conditions via
  // check_if_done.
  if (!rcc.pre())
    return;
  // Read incoming messages for as long as the user's receive loop accepts more
  // messages.
  for (;;) {
    // Reset the timeout each iteration.
    auto rel_tout = bhvr.timeout();
    if (rel_tout == infinite) {
      await_data();
    } else {
      auto abs_tout = std::chrono::high_resolution_clock::now();
      abs_tout += rel_tout;
      if (!await_data(abs_tout)) {
        // Short-circuit "loop body".
        bhvr.handle_timeout();
        if (rcc.post() && rcc.pre())
          continue;
        else
          return;
      }
    }
    // Fetch next message from our mailbox.
    auto ptr = mailbox().pop_front();
    auto t0 = std::chrono::steady_clock::now();
    auto mbox_time = ptr->seconds_until(t0);
    // Skip messages that don't match our message ID.
    if (mid.is_response()) {
      if (mid != ptr->mid) {
        stash_.push(ptr.release());
        continue;
      }
    } else if (ptr->mid.is_response()) {
      stash_.push(ptr.release());
      continue;
    }
    // Automatically unlink from actors after receiving an exit.
    if (auto view = make_const_typed_message_view<exit_msg>(ptr->content()))
      unlink_from(get<0>(view).source);
    // Blocking actors can nest receives => push/pop `current_element_`
    auto prev_element = current_element_;
    current_element_ = ptr.get();
    auto g = detail::scope_guard{
      [&]() noexcept { current_element_ = prev_element; }};
    // Dispatch on the current mailbox element.
    if (consume()) {
      unstash();
      CAF_LOG_FINALIZE_EVENT();
      if (getf(abstract_actor::collects_metrics_flag)) {
        auto& builtins = builtin_metrics();
        telemetry::timer::observe(builtins.processing_time, t0);
        builtins.mailbox_time->observe(mbox_time);
        builtins.mailbox_size->dec();
      }
      // Check whether we are done.
      if (!rcc.post() || !rcc.pre()) {
        return;
      }
      continue;
    }
    // Message was skipped.
    CAF_LOG_SKIP_EVENT();
    stash_.push(ptr.release());
  }
}

void blocking_actor::await_data() {
  if (mailbox().try_block()) {
    std::unique_lock guard{mtx_};
    while (mailbox().blocked())
      cv_.wait(guard);
  }
}

bool blocking_actor::await_data(timeout_type timeout) {
  if (mailbox().try_block()) {
    std::unique_lock guard{mtx_};
    while (mailbox().blocked()) {
      if (cv_.wait_until(guard, timeout) == std::cv_status::timeout) {
        // If we're unable to set the queue from blocked to empty, then there's
        // a new element in the list.
        return !mailbox().try_unblock();
      }
    }
  }
  return true;
}

mailbox_element_ptr blocking_actor::dequeue() {
  if (auto ptr = mailbox().pop_front())
    return ptr;
  await_data();
  return mailbox().pop_front();
}

void blocking_actor::varargs_tup_receive(receive_cond& rcc, message_id mid,
                                         std::tuple<behavior&>& tup) {
  using namespace detail;
  auto& bhvr = std::get<0>(tup);
  if (bhvr.timeout() == infinite) {
    auto fun = make_blocking_behavior(&bhvr);
    receive_impl(rcc, mid, fun);
  } else {
    auto tmp = after(bhvr.timeout()) >> [&] { bhvr.handle_timeout(); };
    auto fun = make_blocking_behavior(&bhvr, std::move(tmp));
    receive_impl(rcc, mid, fun);
  }
}

size_t blocking_actor::attach_functor(const actor& x) {
  return attach_functor(actor_cast<strong_actor_ptr>(x));
}

size_t blocking_actor::attach_functor(const actor_addr& x) {
  return attach_functor(actor_cast<strong_actor_ptr>(x));
}

size_t blocking_actor::attach_functor(const strong_actor_ptr& ptr) {
  if (!ptr)
    return 0;
  actor self{this};
  auto f = [self](const error&) { caf::anon_mail(wait_for_atom_v).send(self); };
  ptr->get()->attach_functor(std::move(f));
  return 1;
}

void blocking_actor::on_cleanup(const error& reason) {
  close_mailbox(reason);
  on_exit();
  return super::on_cleanup(reason);
}

void blocking_actor::unstash() {
  while (auto stashed = stash_.pop())
    mailbox().push_front(mailbox_element_ptr{stashed});
}

void blocking_actor::close_mailbox(const error& reason) {
  if (!mailbox_.closed()) {
    unstash();
    auto dropped = mailbox_.close(reason);
    if (dropped > 0 && metrics_.mailbox_size)
      metrics_.mailbox_size->dec(static_cast<int64_t>(dropped));
  }
}

void blocking_actor::force_close_mailbox() {
  close_mailbox(make_error(exit_reason::unreachable));
}

void blocking_actor::do_unstash(mailbox_element_ptr ptr) {
  mailbox().push_front(std::move(ptr));
}

void blocking_actor::do_receive(message_id mid, behavior& bhvr,
                                timespan timeout) {
  accept_one_cond cond;
  auto tmp = after(timeout) >> [&] {
    auto err = make_message(make_error(sec::request_timeout));
    bhvr(err);
  };
  auto fun = detail::make_blocking_behavior(&bhvr, std::move(tmp));
  receive_impl(cond, mid, fun);
}

} // namespace caf
