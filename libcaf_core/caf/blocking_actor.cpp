// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/blocking_actor.hpp"

#include "caf/actor_system.hpp"
#include "caf/adopt_ref.hpp"
#include "caf/anon_mail.hpp"
#include "caf/callback.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/log/system.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
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
  if (auto* factory = cfg.mbox_factory) {
    mailbox_.reset(factory->make(this), caf::adopt_ref);
  } else {
    mailbox_.reset(new detail::default_mailbox, caf::adopt_ref);
  }
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
  if (auto* mailbox_size = metrics_.mailbox_size) {
    ptr->set_enqueue_time();
    mailbox_size->inc();
  }
  // returns false if mailbox has been closed
  switch (mailbox().push_back(std::move(ptr))) {
    case intrusive::inbox_result::queue_closed: {
      CAF_LOG_REJECT_EVENT();
      detail::actor_system_access{home_system()}.message_rejected(this);
      if (auto* mailbox_size = metrics_.mailbox_size) {
        mailbox_size->dec();
      }
      if (mid.is_request()) {
        detail::sync_request_bouncer srb;
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
                                 detail::private_thread* thread)
    : self_(self), thread_(thread) {
    self_->ctrl()->ref();
  }

  void resume(scheduler* ctx, uint64_t event_id) override {
    detail::current_actor_guard ctx_guard{self_};
    if (event_id == resumable::dispose_event_id) {
      self_->cleanup(make_error(exit_reason::user_shutdown), ctx);
      return;
    }
    self_->context(ctx);
    self_->initialize(ctx);
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
    auto& sys = self_->system();
    sys.release_private_thread(thread_);
    self_->cleanup(std::move(rsn), ctx);
    self_->ctrl()->deref();
  }

  void ref() const noexcept final {
    // nop
  }

  void deref() const noexcept final {
    delete this;
  }

private:
  blocking_actor* self_;
  detail::private_thread* thread_;
};

} // namespace

bool blocking_actor::initialize(scheduler*) {
  return true;
}

local_actor::consume_result blocking_actor::consume(mailbox_element_ptr& what) {
  CAF_ASSERT(what != nullptr);
  CAF_ASSERT(current_behavior_ != nullptr);
  // Blocking actors can nest receives => push/pop `current_element_`
  auto prev_element = current_element_;
  current_element_ = what.get();
  auto g = detail::scope_guard{[this, prev_element]() noexcept { //
    current_element_ = prev_element;
  }};
  detail::default_invoke_result_visitor<blocking_actor> visitor{this};
  if ((*current_behavior_)(visitor, what->payload)) {
    return consume_result::consumed;
  }
  return consume_result::skipped;
}

bool blocking_actor::launch_delayed() {
  return false;
}

void blocking_actor::launch(detail::private_thread* worker, scheduler*) {
  CAF_ASSERT(worker != nullptr);
  detail::current_actor_guard ctx_guard{this};
  auto lg = log::core::trace("");
  worker->resume(make_counted<blocking_actor_runner>(this, worker));
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
  system().await_running_actors_count_equal(getf(is_registered_flag) ? 1 : 0);
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
                                  behavior& bhvr, timespan timeout,
                                  callback<void()>& on_timeout) {
  detail::current_actor_guard ctx_guard{this};
  auto lg = log::core::trace("mid = {}", mid);
  unstash();
  // Check pre-condition once before entering the message consumption loop.
  if (!rcc.pre())
    return;
  // Read incoming messages for as long as the user's receive loop accepts more
  // messages.
  for (;;) {
    // Reset the timeout each iteration.
    if (timeout == infinite) {
      await_data();
    } else {
      auto abs_tout = std::chrono::high_resolution_clock::now();
      abs_tout += timeout;
      if (!await_data(abs_tout)) {
        // Short-circuit "loop body".
        on_timeout();
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
    if (auto view = make_const_typed_message_view<exit_msg>(ptr->content())) {
      unlink_from(get<0>(view).source);
    }
    // Automatically clear incoming edges after receiving a down message.
    if (auto view = make_const_typed_message_view<down_msg>(ptr->content())) {
      clear_incoming_edges(get<0>(view).source);
    }
    // Always set `current_behavior_` and `current_message_id_` before consuming
    // the message, since `consume` may call `receive` again.
    current_behavior_ = &bhvr;
    current_message_id_ = mid;
    bool consumed_msg = false;
    switch (consume(ptr)) {
      case consume_result::consumed:
        unstash();
        consumed_msg = true;
        break;
      case consume_result::terminated:
        return;
      case consume_result::skipped:
        if (mid.is_response()) {
          // Default fallback is skip: deliver unexpected_response to handler.
          detail::default_invoke_result_visitor<blocking_actor> visitor{this};
          auto err = make_error(sec::unexpected_response,
                                std::move(ptr->payload));
          mailbox_element tmp{ptr->sender, ptr->mid,
                              make_message(std::move(err))};
          auto prev = current_element_;
          current_element_ = &tmp;
          auto g = detail::scope_guard{
            [this, prev]() noexcept { current_element_ = prev; }};
          bhvr(visitor, tmp.content());
          unstash();
          consumed_msg = true;
          break;
        }
        CAF_LOG_SKIP_EVENT();
        stash_.push(ptr.release());
        break;
    }
    if (consumed_msg && getf(abstract_actor::collects_metrics_flag)) {
      auto& builtins = builtin_metrics();
      telemetry::timer::observe(builtins.processing_time, t0);
      builtins.mailbox_time->observe(mbox_time);
      builtins.mailbox_size->dec();
    }
    // `accept_one_cond::post()` returns false after one *accepted* message. A
    // skipped message must not end the loop, or we exit before later mail (e.g.
    // a request after an unrelated prior message) is ever handled.
    if (consumed_msg && (!rcc.post() || !rcc.pre())) {
      return;
    }
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
  auto& bhvr = std::get<0>(tup);
  if (bhvr.timeout() == infinite) {
    detail::nop_callback<void()> on_timeout;
    receive_impl(rcc, mid, bhvr, infinite, on_timeout);
  } else {
    auto on_timeout_cb = make_callback([&bhvr] { bhvr.handle_timeout(); });
    receive_impl(rcc, mid, bhvr, bhvr.timeout(), on_timeout_cb);
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
  close_mailbox();
  on_exit();
  return super::on_cleanup(reason);
}

void blocking_actor::unstash() {
  while (auto stashed = stash_.pop())
    mailbox().push_front(mailbox_element_ptr{stashed});
}

void blocking_actor::close_mailbox() {
  if (!mailbox_->closed()) {
    unstash();
    auto dropped = mailbox_->close();
    if (dropped > 0 && metrics_.mailbox_size)
      metrics_.mailbox_size->dec(static_cast<int64_t>(dropped));
  }
}

bool blocking_actor::try_force_close_mailbox() {
  if (mailbox_->close_if_blocked()) {
    // Discard everything in the stash.
    auto dropped = 0;
    detail::sync_request_bouncer bounce;
    while (auto stashed = stash_.pop()) {
      mailbox_element_ptr ptr{stashed};
      bounce(*ptr);
      ++dropped;
    }
    if (dropped > 0 && metrics_.mailbox_size) {
      metrics_.mailbox_size->dec(dropped);
    }
    return true;
  }
  return false;
}

void blocking_actor::do_unstash(mailbox_element_ptr ptr) {
  mailbox().push_front(std::move(ptr));
}

void blocking_actor::receive_impl(message_id mid, behavior& bhvr,
                                  timespan timeout) {
  accept_one_cond cond;
  auto on_timeout_cb = make_callback([&bhvr] {
    auto err = make_message(make_error(sec::request_timeout));
    bhvr(err);
  });
  receive_impl(cond, mid, bhvr, timeout, on_timeout_cb);
}

} // namespace caf
