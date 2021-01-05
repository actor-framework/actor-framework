// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/blocking_actor.hpp"

#include <utility>

#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/logger.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/telemetry/timer.hpp"

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
  : super(cfg.add_flag(local_actor::is_blocking_flag)),
    mailbox_(unit, unit, unit) {
  // nop
}

blocking_actor::~blocking_actor() {
  // avoid weak-vtables warning
}

void blocking_actor::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(getf(is_blocking_flag));
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  CAF_LOG_SEND_EVENT(ptr);
  auto mid = ptr->mid;
  auto src = ptr->sender;
  auto collects_metrics = getf(abstract_actor::collects_metrics_flag);
  if (collects_metrics) {
    ptr->set_enqueue_time();
    metrics_.mailbox_size->inc();
  }
  // returns false if mailbox has been closed
  if (!mailbox().synchronized_push_back(mtx_, cv_, std::move(ptr))) {
    CAF_LOG_REJECT_EVENT();
    home_system().base_metrics().rejected_messages->inc();
    if (collects_metrics)
      metrics_.mailbox_size->dec();
    if (mid.is_request()) {
      detail::sync_request_bouncer srb{exit_reason()};
      srb(src, mid);
    }
  } else {
    CAF_LOG_ACCEPT_EVENT(false);
  }
}

mailbox_element* blocking_actor::peek_at_next_mailbox_element() {
  return mailbox().closed() || mailbox().blocked() ? nullptr : mailbox().peek();
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
    intrusive_ptr_add_ref(self->ctrl());
  }

  resumable::subtype_t subtype() const override {
    return resumable::function_object;
  }

  resumable::resume_result resume(execution_unit* ctx, size_t) override {
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
    try {
      self_->on_exit();
    } catch (...) {
      // simply ignore exception
    }
#else
    self_->act();
    rsn = self_->fail_state();
    self_->on_exit();
#endif
    self_->cleanup(std::move(rsn), ctx);
    intrusive_ptr_release(self_->ctrl());
    ctx->system().release_private_thread(thread_);
    return resumable::done;
  }

  void intrusive_ptr_add_ref_impl() override {
    // nop
  }

  void intrusive_ptr_release_impl() override {
    delete this;
  }

private:
  blocking_actor* self_;
  detail::private_thread* thread_;
};

} // namespace

void blocking_actor::launch(execution_unit*, bool, bool hide) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_LOG_TRACE(CAF_ARG(hide));
  CAF_ASSERT(getf(is_blocking_flag));
  if (!hide)
    register_at_system();
  auto thread = home_system().acquire_private_thread();
  thread->resume(new blocking_actor_runner(this, thread));
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
  CAF_LOG_TRACE("");
  if (initial_behavior_fac_)
    initial_behavior_fac_(this);
}

void blocking_actor::fail_state(error err) {
  fail_state_ = std::move(err);
}

intrusive::task_result
blocking_actor::mailbox_visitor::operator()(mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_LOG_RECEIVE_EVENT((&x));
  CAF_BEFORE_PROCESSING(self, x);
  // Wrap the actual body for the function.
  auto body = [this, &x] {
    auto check_if_done = [&]() -> intrusive::task_result {
      // Stop consuming items when reaching the end of the user-defined receive
      // loop either via post or pre condition.
      if (rcc.post() && rcc.pre())
        return intrusive::task_result::resume;
      done = true;
      return intrusive::task_result::stop;
    };
    // Skip messages that don't match our message ID.
    if (mid.is_response()) {
      if (mid != x.mid) {
        return intrusive::task_result::skip;
      }
    } else if (x.mid.is_response()) {
      return intrusive::task_result::skip;
    }
    // Automatically unlink from actors after receiving an exit.
    if (auto view = make_const_typed_message_view<exit_msg>(x.content()))
      self->unlink_from(get<0>(view).source);
    // Blocking actors can nest receives => push/pop `current_element_`
    auto prev_element = self->current_element_;
    self->current_element_ = &x;
    auto g = detail::make_scope_guard(
      [&] { self->current_element_ = prev_element; });
    // Dispatch on x.
    detail::default_invoke_result_visitor<blocking_actor> visitor{self};
    if (bhvr.nested(visitor, x.content()))
      return check_if_done();
    // Blocking actors can have fallback handlers for catch-all rules.
    auto sres = bhvr.fallback(self->current_element_->payload);
    auto f = detail::make_overload(
      [&](skip_t&) {
        // Response handlers must get re-invoked with an error when
        // receiving an unexpected message.
        if (mid.is_response()) {
          auto err = make_error(sec::unexpected_response, std::move(x.payload));
          mailbox_element tmp{std::move(x.sender), x.mid, std::move(x.stages),
                              make_message(std::move(err))};
          self->current_element_ = &tmp;
          bhvr.nested(tmp.content());
          return check_if_done();
        }
        return intrusive::task_result::skip;
      },
      [&](auto& res) {
        visitor(res);
        return check_if_done();
      });
    return visit(f, sres);
  };
  // Post-process the returned value from the function body.
  if (!self->getf(abstract_actor::collects_metrics_flag)) {
    auto result = body();
    if (result == intrusive::task_result::skip) {
      CAF_AFTER_PROCESSING(self, invoke_message_result::skipped);
      CAF_LOG_SKIP_EVENT();
    } else {
      CAF_AFTER_PROCESSING(self, invoke_message_result::consumed);
      CAF_LOG_FINALIZE_EVENT();
    }
    return result;
  } else {
    auto t0 = std::chrono::steady_clock::now();
    auto mbox_time = x.seconds_until(t0);
    auto result = body();
    if (result == intrusive::task_result::skip) {
      CAF_AFTER_PROCESSING(self, invoke_message_result::skipped);
      CAF_LOG_SKIP_EVENT();
      auto& builtins = self->builtin_metrics();
      telemetry::timer::observe(builtins.processing_time, t0);
      builtins.mailbox_time->observe(mbox_time);
      builtins.mailbox_size->dec();
    } else {
      CAF_AFTER_PROCESSING(self, invoke_message_result::consumed);
      CAF_LOG_FINALIZE_EVENT();
    }
    return result;
  }
}

void blocking_actor::receive_impl(receive_cond& rcc, message_id mid,
                                  detail::blocking_behavior& bhvr) {
  CAF_LOG_TRACE(CAF_ARG(mid));
  // Set to `true` by the visitor when done.
  bool done = false;
  // Make sure each receive sees all mailbox elements.
  mailbox_visitor f{this, done, rcc, mid, bhvr};
  mailbox().flush_cache();
  // Check pre-condition once before entering the message consumption loop. The
  // consumer performs any future check on pre and post conditions via
  // check_if_done.
  if (!rcc.pre())
    return;
  // Read incoming messages for as long as the user's receive loop accepts more
  // messages.
  do {
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
    mailbox_.new_round(3, f);
  } while (!done);
}

void blocking_actor::await_data() {
  mailbox().synchronized_await(mtx_, cv_);
}

bool blocking_actor::await_data(timeout_type timeout) {
  return mailbox().synchronized_await(mtx_, cv_, timeout);
}

mailbox_element_ptr blocking_actor::dequeue() {
  mailbox().flush_cache();
  await_data();
  mailbox().fetch_more();
  auto& qs = mailbox().queue().queues();
  auto result = get<mailbox_policy::urgent_queue_index>(qs).take_front();
  if (!result)
    result = get<mailbox_policy::normal_queue_index>(qs).take_front();
  CAF_ASSERT(result != nullptr);
  return result;
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

sec blocking_actor::build_pipeline(stream_slot, stream_slot,
                                   stream_manager_ptr) {
  CAF_LOG_ERROR("blocking_actor::build_pipeline called");
  return sec::bad_function_call;
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
  auto f = [self](const error&) { caf::anon_send(self, wait_for_atom_v); };
  ptr->get()->attach_functor(std::move(f));
  return 1;
}

bool blocking_actor::cleanup(error&& fail_state, execution_unit* host) {
  if (!mailbox_.closed()) {
    mailbox_.close();
    // TODO: messages that are stuck in the cache can get lost
    detail::sync_request_bouncer bounce{fail_state};
    auto dropped = mailbox_.queue().new_round(1000, bounce).consumed_items;
    while (dropped > 0) {
      if (getf(abstract_actor::collects_metrics_flag)) {
        auto val = static_cast<int64_t>(dropped);
        metrics_.mailbox_size->dec(val);
      }
      dropped = mailbox_.queue().new_round(1000, bounce).consumed_items;
    }
  }
  // Dispatch to parent's `cleanup` function.
  return super::cleanup(std::move(fail_state), host);
}

} // namespace caf
