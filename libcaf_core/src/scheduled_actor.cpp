/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/scheduled_actor.hpp"

#include "caf/actor_ostream.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/inbound_path.hpp"
#include "caf/to_string.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

using namespace std::string_literals;

namespace caf {

// -- related free functions ---------------------------------------------------

result<message> reflect(scheduled_actor*, message_view& x) {
  return x.move_content_to_message();
}

result<message> reflect_and_quit(scheduled_actor* ptr, message_view& x) {
  error err = exit_reason::normal;
  scheduled_actor::default_error_handler(ptr, err);
  return reflect(ptr, x);
}

result<message> print_and_drop(scheduled_actor* ptr, message_view& x) {
  CAF_LOG_WARNING("unexpected message" << CAF_ARG(x.content()));
  aout(ptr) << "*** unexpected message [id: " << ptr->id()
            << ", name: " << ptr->name() << "]: " << x.content().stringify()
            << std::endl;
  return sec::unexpected_message;
}

result<message> drop(scheduled_actor*, message_view&) {
  return sec::unexpected_message;
}

// -- implementation details ---------------------------------------------------

namespace {

template <class T>
void silently_ignore(scheduled_actor*, T&) {
  // nop
}

result<message> drop_after_quit(scheduled_actor* self, message_view&) {
  if (self->current_message_id().is_request())
    return make_error(sec::request_receiver_down);
  return make_message();
}

} // namespace

// -- static helper functions --------------------------------------------------

void scheduled_actor::default_error_handler(scheduled_actor* ptr, error& x) {
  ptr->quit(std::move(x));
}

void scheduled_actor::default_down_handler(scheduled_actor* ptr, down_msg& x) {
  aout(ptr) << "*** unhandled down message [id: " << ptr->id()
            << ", name: " << ptr->name() << "]: " << to_string(x) << std::endl;
}

void scheduled_actor::default_exit_handler(scheduled_actor* ptr, exit_msg& x) {
  if (x.reason)
    default_error_handler(ptr, x.reason);
}

#ifndef CAF_NO_EXCEPTIONS
error scheduled_actor::default_exception_handler(pointer ptr,
                                                 std::exception_ptr& x) {
  CAF_ASSERT(x != nullptr);
  try {
    std::rethrow_exception(x);
  } catch (const std::exception& e) {
    aout(ptr) << "*** unhandled exception: [id: " << ptr->id()
              << ", name: " << ptr->name()
              << ", exception typeid: " << typeid(e).name() << "]: " << e.what()
              << std::endl;
  } catch (...) {
    aout(ptr) << "*** unhandled exception: [id: " << ptr->id()
              << ", name: " << ptr->name() << "]: unknown exception"
              << std::endl;
  }
  return sec::runtime_error;
}
#endif // CAF_NO_EXCEPTIONS

// -- constructors and destructors ---------------------------------------------

scheduled_actor::scheduled_actor(actor_config& cfg)
  : super(cfg),
    mailbox_(unit, unit, unit, unit, unit),
    timeout_id_(0),
    default_handler_(print_and_drop),
    error_handler_(default_error_handler),
    down_handler_(default_down_handler),
    exit_handler_(default_exit_handler),
    private_thread_(nullptr)
#ifndef CAF_NO_EXCEPTIONS
    ,
    exception_handler_(default_exception_handler)
#endif // CAF_NO_EXCEPTIONS
{
  auto& sys_cfg = home_system().config();
  auto interval = sys_cfg.stream_tick_duration();
  CAF_ASSERT(interval.count() > 0);
  stream_ticks_.interval(interval);
  CAF_ASSERT(sys_cfg.stream_max_batch_delay.count() > 0);
  auto div = [](timespan x, timespan y) {
    return static_cast<size_t>(x.count() / y.count());
  };
  max_batch_delay_ticks_ = div(sys_cfg.stream_max_batch_delay, interval);
  CAF_ASSERT(max_batch_delay_ticks_ > 0);
  credit_round_ticks_ = div(sys_cfg.stream_credit_round_interval, interval);
  CAF_ASSERT(credit_round_ticks_ > 0);
  CAF_LOG_DEBUG(CAF_ARG(interval) << CAF_ARG(max_batch_delay_ticks_)
                                  << CAF_ARG(credit_round_ticks_));
}

scheduled_actor::~scheduled_actor() {
  // signalize to the private thread object that it is
  // unreachable and can be destroyed as well
  if (private_thread_ != nullptr)
    private_thread_->notify_self_destroyed();
}

// -- overridden functions of abstract_actor -----------------------------------

void scheduled_actor::enqueue(mailbox_element_ptr ptr, execution_unit* eu) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(!getf(is_blocking_flag));
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  CAF_LOG_SEND_EVENT(ptr);
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  switch (mailbox().push_back(std::move(ptr))) {
    case intrusive::inbox_result::unblocked_reader: {
      CAF_LOG_ACCEPT_EVENT(true);
      // add a reference count to this actor and re-schedule it
      intrusive_ptr_add_ref(ctrl());
      if (getf(is_detached_flag)) {
        CAF_ASSERT(private_thread_ != nullptr);
        private_thread_->resume();
      } else {
        if (eu != nullptr)
          eu->exec_later(this);
        else
          home_system().scheduler().enqueue(this);
      }
      break;
    }
    case intrusive::inbox_result::queue_closed: {
      CAF_LOG_REJECT_EVENT();
      if (mid.is_request()) {
        detail::sync_request_bouncer f{exit_reason()};
        f(sender, mid);
      }
      break;
    }
    case intrusive::inbox_result::success:
      // enqueued to a running actors' mailbox; nothing to do
      CAF_LOG_ACCEPT_EVENT(false);
      break;
  }
}
mailbox_element* scheduled_actor::peek_at_next_mailbox_element() {
  return mailbox().closed() || mailbox().blocked() ? nullptr : mailbox().peek();
}

// -- overridden functions of local_actor --------------------------------------

const char* scheduled_actor::name() const {
  return "scheduled_actor";
}

void scheduled_actor::launch(execution_unit* eu, bool lazy, bool hide) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
  CAF_ASSERT(!getf(is_blocking_flag));
  if (!hide)
    register_at_system();
  if (getf(is_detached_flag)) {
    private_thread_ = new detail::private_thread(this);
    private_thread_->start();
    return;
  }
  CAF_ASSERT(eu != nullptr);
  // do not schedule immediately when spawned with `lazy_init`
  // mailbox could be set to blocked
  if (lazy && mailbox().try_block())
    return;
  // scheduler has a reference count to the actor as long as
  // it is waiting to get scheduled
  intrusive_ptr_add_ref(ctrl());
  eu->exec_later(this);
}

bool scheduled_actor::cleanup(error&& fail_state, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(fail_state));
  // Shutdown hosting thread when running detached.
  if (getf(is_detached_flag)) {
    CAF_ASSERT(private_thread_ != nullptr);
    private_thread_->shutdown();
  }
  // Clear state for open requests.
  awaited_responses_.clear();
  multiplexed_responses_.clear();
  // Clear state for open streams.
  for (auto& kvp : stream_managers_)
    kvp.second->stop(fail_state);
  for (auto& kvp : pending_stream_managers_)
    kvp.second->stop(fail_state);
  stream_managers_.clear();
  pending_stream_managers_.clear();
  get_downstream_queue().cleanup();
  // Clear mailbox.
  if (!mailbox_.closed()) {
    mailbox_.close();
    get_normal_queue().flush_cache();
    get_urgent_queue().flush_cache();
    detail::sync_request_bouncer bounce{fail_state};
    while (mailbox_.queue().new_round(1000, bounce).consumed_items)
      ; // nop
  }
  // Dispatch to parent's `cleanup` function.
  return super::cleanup(std::move(fail_state), host);
}

// -- overridden functions of resumable ----------------------------------------

resumable::subtype_t scheduled_actor::subtype() const {
  return resumable::scheduled_actor;
}

void scheduled_actor::intrusive_ptr_add_ref_impl() {
  intrusive_ptr_add_ref(ctrl());
}

void scheduled_actor::intrusive_ptr_release_impl() {
  intrusive_ptr_release(ctrl());
}

namespace {

// TODO: replace with generic lambda when switching to C++14
struct upstream_msg_visitor {
  scheduled_actor* selfptr;
  upstream_msg& um;

  template <class T>
  void operator()(T& x) {
    selfptr->handle_upstream_msg(um.slots, um.sender, x);
  }
};

} // namespace

intrusive::task_result
scheduled_actor::mailbox_visitor::operator()(size_t, upstream_queue&,
                                             mailbox_element& x) {
  CAF_ASSERT(x.content().match_elements<upstream_msg>());
  self->current_mailbox_element(&x);
  CAF_LOG_RECEIVE_EVENT((&x));
  CAF_BEFORE_PROCESSING(self, x);
  auto& um = x.content().get_mutable_as<upstream_msg>(0);
  upstream_msg_visitor f{self, um};
  visit(f, um.content);
  CAF_AFTER_PROCESSING(self, invoke_message_result::consumed);
  return ++handled_msgs < max_throughput ? intrusive::task_result::resume
                                         : intrusive::task_result::stop_all;
}

namespace {

// TODO: replace with generic lambda when switching to C++14
struct downstream_msg_visitor {
  scheduled_actor* selfptr;
  scheduled_actor::downstream_queue& qs_ref;
  policy::downstream_messages::nested_queue_type& q_ref;
  downstream_msg& dm;

  template <class T>
  intrusive::task_result operator()(T& x) {
    CAF_LOG_TRACE(CAF_ARG(x));
    auto& inptr = q_ref.policy().handler;
    if (inptr == nullptr)
      return intrusive::task_result::stop;
    // Do *not* store a reference here since we potentially reset `inptr`.
    auto mgr = inptr->mgr;
    inptr->handle(x);
    // The sender slot can be 0. This is the case for forced_close or
    // forced_drop messages from stream aborters.
    CAF_ASSERT(
      inptr->slots == dm.slots
      || (dm.slots.sender == 0 && dm.slots.receiver == inptr->slots.receiver));
    // TODO: replace with `if constexpr` when switching to C++17
    if (std::is_same<T, downstream_msg::close>::value
        || std::is_same<T, downstream_msg::forced_close>::value) {
      inptr.reset();
      qs_ref.erase_later(dm.slots.receiver);
      selfptr->erase_stream_manager(dm.slots.receiver);
      if (mgr->done()) {
        CAF_LOG_DEBUG("path is done receiving and closes its manager");
        selfptr->erase_stream_manager(mgr);
        mgr->stop();
      }
      return intrusive::task_result::stop;
    } else if (mgr->done()) {
      CAF_LOG_DEBUG("path is done receiving and closes its manager");
      selfptr->erase_stream_manager(mgr);
      mgr->stop();
      return intrusive::task_result::stop;
    }
    return intrusive::task_result::resume;
  }
};

} // namespace

intrusive::task_result scheduled_actor::mailbox_visitor::operator()(
  size_t, downstream_queue& qs, stream_slot,
  policy::downstream_messages::nested_queue_type& q, mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(handled_msgs));
  self->current_mailbox_element(&x);
  CAF_LOG_RECEIVE_EVENT((&x));
  CAF_BEFORE_PROCESSING(self, x);
  CAF_ASSERT(x.content().match_elements<downstream_msg>());
  auto& dm = x.content().get_mutable_as<downstream_msg>(0);
  downstream_msg_visitor f{self, qs, q, dm};
  auto res = visit(f, dm.content);
  CAF_AFTER_PROCESSING(self, invoke_message_result::consumed);
  return ++handled_msgs < max_throughput ? res
                                         : intrusive::task_result::stop_all;
}

intrusive::task_result
scheduled_actor::mailbox_visitor::operator()(mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(handled_msgs));
  switch (self->reactivate(x)) {
    case activation_result::terminated:
      return intrusive::task_result::stop;
    case activation_result::success:
      return ++handled_msgs < max_throughput ? intrusive::task_result::resume
                                             : intrusive::task_result::stop_all;
    case activation_result::skipped:
      return intrusive::task_result::skip;
    default:
      return intrusive::task_result::resume;
  }
}

resumable::resume_result scheduled_actor::resume(execution_unit* ctx,
                                                 size_t max_throughput) {
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE(CAF_ARG(max_throughput));
  if (!activate(ctx))
    return resumable::done;
  size_t handled_msgs = 0;
  actor_clock::time_point tout{actor_clock::duration_type{0}};
  auto reset_timeouts_if_needed = [&] {
    // Set a new receive timeout if we called our behavior at least once.
    if (handled_msgs > 0)
      set_receive_timeout();
    // Set a new stream timeout.
    if (!stream_managers_.empty()) {
      // Make sure we call `advance_streams` at least once.
      if (tout.time_since_epoch().count() != 0)
        tout = advance_streams(clock().now());
      set_stream_timeout(tout);
    }
  };
  mailbox_visitor f{this, handled_msgs, max_throughput};
  mailbox_element_ptr ptr;
  // Timeout for calling `advance_streams`.
  while (handled_msgs < max_throughput) {
    CAF_LOG_DEBUG("start new DRR round");
    // TODO: maybe replace '3' with configurable / adaptive value?
    // Dispatch on the different message categories in our mailbox.
    if (!mailbox_.new_round(3, f).consumed_items) {
      reset_timeouts_if_needed();
      if (mailbox().try_block())
        return resumable::awaiting_message;
    }
    // Check whether the visitor left the actor without behavior.
    if (finalize()) {
      return resumable::done;
    }
    // Advance streams, i.e., try to generating credit or to emit batches.
    auto now = clock().now();
    if (now >= tout)
      tout = advance_streams(now);
  }
  CAF_LOG_DEBUG("max throughput reached");
  reset_timeouts_if_needed();
  if (mailbox().try_block())
    return resumable::awaiting_message;
  // time's up
  return resumable::resume_later;
}

// -- scheduler callbacks ------------------------------------------------------

proxy_registry* scheduled_actor::proxy_registry_ptr() {
  return nullptr;
}

// -- state modifiers ----------------------------------------------------------

void scheduled_actor::quit(error x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  // Make sure repeated calls to quit don't do anything.
  if (getf(is_shutting_down_flag))
    return;
  // Mark this actor as about-to-die.
  setf(is_shutting_down_flag);
  // Store shutdown reason.
  fail_state_ = std::move(x);
  // Clear state for handling regular messages.
  bhvr_stack_.clear();
  awaited_responses_.clear();
  multiplexed_responses_.clear();
  // Ignore future exit, down and error messages.
  set_exit_handler(silently_ignore<exit_msg>);
  set_down_handler(silently_ignore<down_msg>);
  set_error_handler(silently_ignore<error>);
  // Drop future messages and produce sec::request_receiver_down for requests.
  set_default_handler(drop_after_quit);
  // Tell all streams to shut down.
  std::vector<stream_manager_ptr> managers;
  for (auto& smm : {stream_managers_, pending_stream_managers_})
    for (auto& kvp : smm)
      managers.emplace_back(kvp.second);
  // Make sure we shutdown each manager exactly once.
  std::sort(managers.begin(), managers.end());
  auto e = std::unique(managers.begin(), managers.end());
  for (auto i = managers.begin(); i != e; ++i) {
    auto& mgr = *i;
    mgr->shutdown();
    // Managers can become done after calling quit if they were continuous.
    if (mgr->done()) {
      mgr->stop();
      erase_stream_manager(mgr);
    }
  }
}

// -- timeout management -------------------------------------------------------

uint64_t scheduled_actor::set_receive_timeout(actor_clock::time_point x) {
  CAF_LOG_TRACE(x);
  setf(has_timeout_flag);
  return set_timeout("receive", x);
}

uint64_t scheduled_actor::set_receive_timeout() {
  CAF_LOG_TRACE("");
  if (bhvr_stack_.empty())
    return 0;
  auto timeout = bhvr_stack_.back().timeout();
  if (timeout == infinite) {
    unsetf(has_timeout_flag);
    return 0;
  }
  if (timeout == timespan{0}) {
    // immediately enqueue timeout message if duration == 0s
    auto id = ++timeout_id_;
    auto type = "receive"s;
    eq_impl(make_message_id(), nullptr, context(), timeout_msg{type, id});
    return id;
  }
  auto t = clock().now();
  t += timeout;
  return set_receive_timeout(t);
}

void scheduled_actor::reset_receive_timeout(uint64_t timeout_id) {
  if (is_active_receive_timeout(timeout_id))
    unsetf(has_timeout_flag);
}

bool scheduled_actor::is_active_receive_timeout(uint64_t tid) const {
  return getf(has_timeout_flag) && timeout_id_ == tid;
}

uint64_t scheduled_actor::set_stream_timeout(actor_clock::time_point x) {
  CAF_LOG_TRACE(x);
  // Do not request 'infinite' timeouts.
  if (x == actor_clock::time_point::max()) {
    CAF_LOG_DEBUG("drop infinite timeout");
    return 0;
  }
  // Do not request a timeout if all streams are idle.
  std::vector<stream_manager_ptr> mgrs;
  for (auto& kvp : stream_managers_)
    mgrs.emplace_back(kvp.second);
  std::sort(mgrs.begin(), mgrs.end());
  auto e = std::unique(mgrs.begin(), mgrs.end());
  auto idle = [=](const stream_manager_ptr& y) { return y->idle(); };
  if (std::all_of(mgrs.begin(), e, idle)) {
    CAF_LOG_DEBUG("suppress stream timeout");
    return 0;
  }
  // Delegate call.
  return set_timeout("stream", x);
}

// -- message processing -------------------------------------------------------

void scheduled_actor::add_awaited_response_handler(message_id response_id,
                                                   behavior bhvr) {
  if (bhvr.timeout() != infinite)
    request_response_timeout(bhvr.timeout(), response_id);
  awaited_responses_.emplace_front(response_id, std::move(bhvr));
}

void scheduled_actor::add_multiplexed_response_handler(message_id response_id,
                                                       behavior bhvr) {
  if (bhvr.timeout() != infinite)
    request_response_timeout(bhvr.timeout(), response_id);
  multiplexed_responses_.emplace(response_id, std::move(bhvr));
}

scheduled_actor::message_category
scheduled_actor::categorize(mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  auto& content = x.content();
  if (content.match_elements<sys_atom, get_atom, std::string>()) {
    auto rp = make_response_promise();
    if (!rp.pending()) {
      CAF_LOG_WARNING("received anonymous ('get', 'sys', $key) message");
      return message_category::internal;
    }
    auto& what = content.get_as<std::string>(2);
    if (what == "info") {
      CAF_LOG_DEBUG("reply to 'info' message");
      rp.deliver(ok_atom_v, std::move(what), strong_actor_ptr{ctrl()}, name());
    } else {
      rp.deliver(make_error(sec::unsupported_sys_key));
    }
    return message_category::internal;
  }
  if (content.match_elements<timeout_msg>()) {
    CAF_ASSERT(x.mid.is_async());
    auto& tm = content.get_as<timeout_msg>(0);
    auto tid = tm.timeout_id;
    if (tm.type == "receive") {
      CAF_LOG_DEBUG("handle ordinary timeout message");
      if (is_active_receive_timeout(tid) && !bhvr_stack_.empty())
        bhvr_stack_.back().handle_timeout();
    } else {
      CAF_ASSERT(tm.type == "stream");
      CAF_LOG_DEBUG("handle stream timeout message");
      set_stream_timeout(advance_streams(clock().now()));
    }
    return message_category::internal;
  }
  if (content.match_elements<exit_msg>()) {
    auto em = content.move_if_unshared<exit_msg>(0);
    // make sure to get rid of attachables if they're no longer needed
    unlink_from(em.source);
    // exit_reason::kill is always fatal and also aborts streams.
    if (em.reason == exit_reason::kill) {
      quit(std::move(em.reason));
      std::vector<stream_manager_ptr> xs;
      for (auto& kvp : stream_managers_)
        xs.emplace_back(kvp.second);
      for (auto& kvp : pending_stream_managers_)
        xs.emplace_back(kvp.second);
      std::sort(xs.begin(), xs.end());
      auto last = std::unique(xs.begin(), xs.end());
      std::for_each(xs.begin(), last, [&](stream_manager_ptr& mgr) {
        mgr->stop(exit_reason::kill);
      });
      stream_managers_.clear();
      pending_stream_managers_.clear();
    } else {
      call_handler(exit_handler_, this, em);
    }
    return message_category::internal;
  }
  if (content.match_elements<down_msg>()) {
    auto dm = content.move_if_unshared<down_msg>(0);
    call_handler(down_handler_, this, dm);
    return message_category::internal;
  }
  if (content.match_elements<error>()) {
    auto err = content.move_if_unshared<error>(0);
    call_handler(error_handler_, this, err);
    return message_category::internal;
  }
  if (content.match_elements<open_stream_msg>()) {
    return handle_open_stream_msg(x) != invoke_message_result::skipped
             ? message_category::internal
             : message_category::skipped;
  }
  return message_category::ordinary;
}

invoke_message_result scheduled_actor::consume(mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  current_element_ = &x;
  CAF_LOG_RECEIVE_EVENT(current_element_);
  CAF_BEFORE_PROCESSING(this, x);
  // Wrap the actual body for the function.
  auto body = [this, &x] {
    // Helper function for dispatching a message to a response handler.
    using ptr_t = scheduled_actor*;
    using fun_t = bool (*)(ptr_t, behavior&, mailbox_element&);
    auto ordinary_invoke = [](ptr_t, behavior& f, mailbox_element& in) -> bool {
      return f(in.content()) != none;
    };
    auto select_invoke_fun = [&]() -> fun_t { return ordinary_invoke; };
    // Short-circuit awaited responses.
    if (!awaited_responses_.empty()) {
      auto invoke = select_invoke_fun();
      auto& pr = awaited_responses_.front();
      // skip all messages until we receive the currently awaited response
      if (x.mid != pr.first)
        return invoke_message_result::skipped;
      auto f = std::move(pr.second);
      awaited_responses_.pop_front();
      if (!invoke(this, f, x)) {
        // try again with error if first attempt failed
        auto msg = make_message(
          make_error(sec::unexpected_response, x.move_content_to_message()));
        f(msg);
      }
      return invoke_message_result::consumed;
    }
    // Handle multiplexed responses.
    if (x.mid.is_response()) {
      auto invoke = select_invoke_fun();
      auto mrh = multiplexed_responses_.find(x.mid);
      // neither awaited nor multiplexed, probably an expired timeout
      if (mrh == multiplexed_responses_.end())
        return invoke_message_result::dropped;
      auto bhvr = std::move(mrh->second);
      multiplexed_responses_.erase(mrh);
      if (!invoke(this, bhvr, x)) {
        // try again with error if first attempt failed
        auto msg = make_message(
          make_error(sec::unexpected_response, x.move_content_to_message()));
        bhvr(msg);
      }
      return invoke_message_result::consumed;
    }
    // Dispatch on the content of x.
    switch (categorize(x)) {
      case message_category::skipped:
        return invoke_message_result::skipped;
      case message_category::internal:
        CAF_LOG_DEBUG("handled system message");
        return invoke_message_result::consumed;
      case message_category::ordinary: {
        detail::default_invoke_result_visitor<scheduled_actor> visitor{this};
        bool skipped = false;
        auto had_timeout = getf(has_timeout_flag);
        if (had_timeout)
          unsetf(has_timeout_flag);
        // restore timeout at scope exit if message was skipped
        auto timeout_guard = detail::make_scope_guard([&] {
          if (skipped && had_timeout)
            setf(has_timeout_flag);
        });
        auto call_default_handler = [&] {
          auto sres = call_handler(default_handler_, this, x);
          switch (sres.flag) {
            default:
              break;
            case rt_error:
            case rt_value:
              visitor.visit(sres);
              break;
            case rt_skip:
              skipped = true;
          }
        };
        if (bhvr_stack_.empty()) {
          call_default_handler();
          return !skipped ? invoke_message_result::consumed
                          : invoke_message_result::skipped;
        }
        auto& bhvr = bhvr_stack_.back();
        switch (bhvr(visitor, x.content())) {
          default:
            break;
          case match_case::skip:
            skipped = true;
            break;
          case match_case::no_match:
            call_default_handler();
        }
        return !skipped ? invoke_message_result::consumed
                        : invoke_message_result::skipped;
      }
    }
    // Unreachable.
    CAF_CRITICAL("invalid message type");
  };
  // Post-process the returned value from the function body.
  auto result = body();
  CAF_AFTER_PROCESSING(this, result);
  CAF_LOG_SKIP_OR_FINALIZE_EVENT(result);
  return result;
}

/// Tries to consume `x`.
void scheduled_actor::consume(mailbox_element_ptr x) {
  switch (consume(*x)) {
    default:
      break;
    case invoke_message_result::skipped:
      push_to_cache(std::move(x));
  }
}

bool scheduled_actor::activate(execution_unit* ctx) {
  CAF_LOG_TRACE("");
  CAF_ASSERT(ctx != nullptr);
  CAF_ASSERT(!getf(is_blocking_flag));
  context(ctx);
  if (getf(is_initialized_flag) && !alive()) {
    CAF_LOG_ERROR("activate called on a terminated actor");
    return false;
  }
#ifndef CAF_NO_EXCEPTIONS
  try {
#endif // CAF_NO_EXCEPTIONS
    if (!getf(is_initialized_flag)) {
      initialize();
      if (finalize()) {
        CAF_LOG_DEBUG("finalize() returned true right after make_behavior()");
        return false;
      }
      CAF_LOG_DEBUG("initialized actor:" << CAF_ARG(name()));
    }
#ifndef CAF_NO_EXCEPTIONS
  } catch (...) {
    CAF_LOG_ERROR("actor died during initialization");
    auto eptr = std::current_exception();
    quit(call_handler(exception_handler_, this, eptr));
    finalize();
    return false;
  }
#endif // CAF_NO_EXCEPTIONS
  return true;
}

auto scheduled_actor::activate(execution_unit* ctx, mailbox_element& x)
  -> activation_result {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (!activate(ctx))
    return activation_result::terminated;
  auto res = reactivate(x);
  if (res == activation_result::success)
    set_receive_timeout();
  return res;
}

auto scheduled_actor::reactivate(mailbox_element& x) -> activation_result {
  CAF_LOG_TRACE(CAF_ARG(x));
#ifndef CAF_NO_EXCEPTIONS
  try {
#endif // CAF_NO_EXCEPTIONS
    switch (consume(x)) {
      case invoke_message_result::dropped:
        return activation_result::dropped;
      case invoke_message_result::consumed:
        bhvr_stack_.cleanup();
        if (finalize()) {
          CAF_LOG_DEBUG("actor finalized");
          return activation_result::terminated;
        }
        return activation_result::success;
      case invoke_message_result::skipped:
        return activation_result::skipped;
    }
#ifndef CAF_NO_EXCEPTIONS
  } catch (std::exception& e) {
    CAF_LOG_INFO("actor died because of an exception, what: " << e.what());
    static_cast<void>(e); // keep compiler happy when not logging
    auto eptr = std::current_exception();
    quit(call_handler(exception_handler_, this, eptr));
  } catch (...) {
    CAF_LOG_INFO("actor died because of an unknown exception");
    auto eptr = std::current_exception();
    quit(call_handler(exception_handler_, this, eptr));
  }
  finalize();
  return activation_result::terminated;
#endif // CAF_NO_EXCEPTIONS
}

// -- behavior management ----------------------------------------------------

void scheduled_actor::do_become(behavior bhvr, bool discard_old) {
  if (getf(is_terminated_flag | is_shutting_down_flag)) {
    CAF_LOG_WARNING("called become() on a terminated actor");
    return;
  }
  if (discard_old && !bhvr_stack_.empty())
    bhvr_stack_.pop_back();
  // request_timeout simply resets the timeout when it's invalid
  if (bhvr)
    bhvr_stack_.push_back(std::move(bhvr));
  set_receive_timeout();
}

bool scheduled_actor::finalize() {
  CAF_LOG_TRACE("");
  // Repeated calls always return `true` but have no side effects.
  if (getf(is_cleaned_up_flag))
    return true;
  // An actor is considered alive as long as it has a behavior and didn't set
  // the terminated flag.
  if (alive())
    return false;
  CAF_LOG_DEBUG("actor has no behavior and is ready for cleanup");
  CAF_ASSERT(!has_behavior());
  on_exit();
  bhvr_stack_.cleanup();
  cleanup(std::move(fail_state_), context());
  CAF_ASSERT(getf(is_cleaned_up_flag));
  return true;
}

void scheduled_actor::push_to_cache(mailbox_element_ptr ptr) {
  using namespace intrusive;
  auto& p = mailbox_.queue().policy();
  auto& qs = mailbox_.queue().queues();
  // TODO: use generic lambda to avoid code duplication when switching to C++14
  if (p.id_of(*ptr) == normal_queue_index) {
    auto& q = std::get<normal_queue_index>(qs);
    q.inc_total_task_size(q.policy().task_size(*ptr));
    q.cache().push_back(ptr.release());
  } else {
    auto& q = std::get<normal_queue_index>(qs);
    q.inc_total_task_size(q.policy().task_size(*ptr));
    q.cache().push_back(ptr.release());
  }
}

scheduled_actor::normal_queue& scheduled_actor::get_normal_queue() {
  return get<normal_queue_index>(mailbox_.queue().queues());
}

scheduled_actor::upstream_queue& scheduled_actor::get_upstream_queue() {
  return get<upstream_queue_index>(mailbox_.queue().queues());
}

scheduled_actor::downstream_queue& scheduled_actor::get_downstream_queue() {
  return get<downstream_queue_index>(mailbox_.queue().queues());
}

scheduled_actor::urgent_queue& scheduled_actor::get_urgent_queue() {
  return get<urgent_queue_index>(mailbox_.queue().queues());
}

inbound_path* scheduled_actor::make_inbound_path(stream_manager_ptr mgr,
                                                 stream_slots slots,
                                                 strong_actor_ptr sender,
                                                 rtti_pair rtti) {
  static constexpr size_t queue_index = downstream_queue_index;
  using policy_type = policy::downstream_messages::nested;
  auto& qs = get<queue_index>(mailbox_.queue().queues()).queues();
  auto res = qs.emplace(slots.receiver, policy_type{nullptr});
  if (!res.second)
    return nullptr;
  auto path = new inbound_path(std::move(mgr), slots, std::move(sender), rtti);
  res.first->second.policy().handler.reset(path);
  return path;
}

void scheduled_actor::erase_inbound_path_later(stream_slot slot) {
  CAF_LOG_TRACE(CAF_ARG(slot));
  get_downstream_queue().erase_later(slot);
}

void scheduled_actor::erase_inbound_path_later(stream_slot slot, error reason) {
  CAF_LOG_TRACE(CAF_ARG(slot) << CAF_ARG(reason));
  auto& q = get_downstream_queue();
  auto e = q.queues().end();
  auto i = q.queues().find(slot);
  if (i != e) {
    auto& path = i->second.policy().handler;
    if (path != nullptr) {
      if (reason == none)
        path->emit_regular_shutdown(this);
      else
        path->emit_irregular_shutdown(this, std::move(reason));
    }
    q.erase_later(slot);
  }
}

void scheduled_actor::erase_inbound_paths_later(const stream_manager* ptr) {
  CAF_LOG_TRACE("");
  for (auto& kvp : get_downstream_queue().queues()) {
    auto& path = kvp.second.policy().handler;
    if (path != nullptr && path->mgr == ptr)
      erase_inbound_path_later(kvp.first);
  }
}

void scheduled_actor::erase_inbound_paths_later(const stream_manager* ptr,
                                                error reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  using fn = void (*)(local_actor*, inbound_path&, error&);
  fn regular = [](local_actor* self, inbound_path& in, error&) {
    in.emit_regular_shutdown(self);
  };
  fn irregular = [](local_actor* self, inbound_path& in, error& rsn) {
    in.emit_irregular_shutdown(self, rsn);
  };
  auto f = reason == none ? regular : irregular;
  for (auto& kvp : get_downstream_queue().queues()) {
    auto& path = kvp.second.policy().handler;
    if (path != nullptr && path->mgr == ptr) {
      f(this, *path, reason);
      erase_inbound_path_later(kvp.first);
    }
  }
}

void scheduled_actor::handle_upstream_msg(stream_slots slots,
                                          actor_addr& sender,
                                          upstream_msg::ack_open& x) {
  CAF_IGNORE_UNUSED(sender);
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(sender) << CAF_ARG(x));
  CAF_ASSERT(sender == x.rebind_to);
  // Get the manager for that stream, move it from `pending_managers_` to
  // `managers_`, and handle `x`.
  auto i = pending_stream_managers_.find(slots.receiver);
  if (i == pending_stream_managers_.end()) {
    CAF_LOG_WARNING("found no corresponding manager for received ack_open");
    return;
  }
  auto ptr = std::move(i->second);
  pending_stream_managers_.erase(i);
  if (!add_stream_manager(slots.receiver, ptr)) {
    CAF_LOG_WARNING("unable to add stream manager after receiving ack_open");
    return;
  }
  ptr->handle(slots, x);
}

uint64_t scheduled_actor::set_timeout(std::string type,
                                      actor_clock::time_point x) {
  CAF_LOG_TRACE(CAF_ARG(type) << CAF_ARG(x));
  auto id = ++timeout_id_;
  CAF_LOG_DEBUG("set timeout:" << CAF_ARG(type) << CAF_ARG(x));
  clock().set_ordinary_timeout(x, this, std::move(type), id);
  return id;
}

stream_slot scheduled_actor::next_slot() {
  stream_slot result = 1;
  auto nslot = [](const stream_manager_map& x) -> stream_slot {
    return x.rbegin()->first + 1;
  };
  if (!stream_managers_.empty())
    result = std::max(nslot(stream_managers_), result);
  if (!pending_stream_managers_.empty())
    result = std::max(nslot(pending_stream_managers_), result);
  return result;
}

void scheduled_actor::assign_slot(stream_slot x, stream_manager_ptr mgr) {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (stream_managers_.empty())
    stream_ticks_.start(clock().now());
  CAF_ASSERT(stream_managers_.count(x) == 0);
  stream_managers_.emplace(x, std::move(mgr));
}

void scheduled_actor::assign_pending_slot(stream_slot x,
                                          stream_manager_ptr mgr) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(pending_stream_managers_.count(x) == 0);
  pending_stream_managers_.emplace(x, std::move(mgr));
}

stream_slot scheduled_actor::assign_next_slot_to(stream_manager_ptr mgr) {
  CAF_LOG_TRACE("");
  auto x = next_slot();
  assign_slot(x, std::move(mgr));
  return x;
}

stream_slot
scheduled_actor::assign_next_pending_slot_to(stream_manager_ptr mgr) {
  CAF_LOG_TRACE("");
  auto x = next_slot();
  assign_pending_slot(x, std::move(mgr));
  return x;
}

bool scheduled_actor::add_stream_manager(stream_slot id,
                                         stream_manager_ptr mgr) {
  CAF_LOG_TRACE(CAF_ARG(id));
  if (stream_managers_.empty())
    stream_ticks_.start(clock().now());
  return stream_managers_.emplace(id, std::move(mgr)).second;
}

void scheduled_actor::erase_stream_manager(stream_slot id) {
  CAF_LOG_TRACE(CAF_ARG(id));
  if (stream_managers_.erase(id) != 0 && stream_managers_.empty())
    stream_ticks_.stop();
  CAF_LOG_DEBUG(CAF_ARG2("stream_managers_.size", stream_managers_.size()));
}

void scheduled_actor::erase_pending_stream_manager(stream_slot id) {
  CAF_LOG_TRACE(CAF_ARG(id));
  pending_stream_managers_.erase(id);
}

void scheduled_actor::erase_stream_manager(const stream_manager_ptr& mgr) {
  CAF_LOG_TRACE("");
  if (!stream_managers_.empty()) {
    auto i = stream_managers_.begin();
    auto e = stream_managers_.end();
    while (i != e)
      if (i->second == mgr)
        i = stream_managers_.erase(i);
      else
        ++i;
    if (stream_managers_.empty())
      stream_ticks_.stop();
  }
  { // Lifetime scope of second iterator pair.
    auto i = pending_stream_managers_.begin();
    auto e = pending_stream_managers_.end();
    while (i != e)
      if (i->second == mgr)
        i = pending_stream_managers_.erase(i);
      else
        ++i;
  }
  CAF_LOG_DEBUG(CAF_ARG2("stream_managers_.size", stream_managers_.size())
                << CAF_ARG2("pending_stream_managers_.size",
                            pending_stream_managers_.size()));
}

invoke_message_result
scheduled_actor::handle_open_stream_msg(mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  // Fetches a stream manager from a behavior.
  struct visitor : detail::invoke_result_visitor {
    void operator()() override {
      // nop
    }

    void operator()(error&) override {
      // nop
    }

    void operator()(message&) override {
      // nop
    }

    void operator()(const none_t&) override {
      // nop
    }
  };
  // Extract the handshake part of the message.
  CAF_ASSERT(x.content().match_elements<open_stream_msg>());
  auto& osm = x.content().get_mutable_as<open_stream_msg>(0);
  visitor f;
  // Utility lambda for aborting the stream on error.
  auto fail = [&](sec y, const char* reason) {
    stream_slots path_id{osm.slot, 0};
    inbound_path::emit_irregular_shutdown(this, path_id, osm.prev_stage,
                                          make_error(y, reason));
    auto rp = make_response_promise();
    rp.deliver(sec::stream_init_failed);
  };
  // Utility for invoking the default handler.
  auto fallback = [&] {
    auto sres = call_handler(default_handler_, this, x);
    switch (sres.flag) {
      default:
        CAF_LOG_DEBUG(
          "default handler was called for open_stream_msg:" << osm.msg);
        fail(sec::stream_init_failed, "dropped open_stream_msg (no match)");
        return invoke_message_result::dropped;
      case rt_skip:
        CAF_LOG_DEBUG("default handler skipped open_stream_msg:" << osm.msg);
        return invoke_message_result::skipped;
    }
  };
  // Invoke behavior and dispatch on the result.
  auto& bs = bhvr_stack();
  if (bs.empty())
    return fallback();
  auto res = (bs.back())(f, osm.msg);
  switch (res) {
    case match_case::result::no_match:
      CAF_LOG_DEBUG("no match in behavior, fall back to default handler");
      return fallback();
    case match_case::result::match: {
      return invoke_message_result::consumed;
    }
    default:
      CAF_LOG_DEBUG("behavior skipped open_stream_msg:" << osm.msg);
      return invoke_message_result::skipped; // nop
  }
}

actor_clock::time_point
scheduled_actor::advance_streams(actor_clock::time_point now) {
  CAF_LOG_TRACE("");
  if (!stream_ticks_.started()) {
    CAF_LOG_DEBUG("tick emitter not started yet");
    return actor_clock::time_point::max();
  }
  /// Advance time for driving forced batches and credit.
  auto bitmask = stream_ticks_.timeouts(now, {max_batch_delay_ticks_,
                                              credit_round_ticks_});
  // Force batches on all output paths.
  if ((bitmask & 0x01) != 0) {
    for (auto& kvp : stream_managers_)
      kvp.second->out().force_emit_batches();
  }
  // Fill up credit on each input path.
  if ((bitmask & 0x02) != 0) {
    CAF_LOG_DEBUG("new credit round");
    auto cycle = stream_ticks_.interval();
    cycle *= static_cast<decltype(cycle)::rep>(credit_round_ticks_);
    auto& qs = get_downstream_queue().queues();
    for (auto& kvp : qs) {
      auto inptr = kvp.second.policy().handler.get();
      if (inptr != nullptr) {
        auto tts = static_cast<int32_t>(kvp.second.total_task_size());
        inptr->emit_ack_batch(this, tts, now, cycle);
      }
    }
  }
  return stream_ticks_.next_timeout(now, {max_batch_delay_ticks_,
                                          credit_round_ticks_});
}

} // namespace caf
