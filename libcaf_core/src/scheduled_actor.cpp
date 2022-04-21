// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scheduled_actor.hpp"

#include "caf/action.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

using namespace std::string_literals;

namespace caf {

// -- related free functions ---------------------------------------------------

skippable_result reflect(scheduled_actor*, message& msg) {
  return std::move(msg);
}

skippable_result reflect_and_quit(scheduled_actor* ptr, message& msg) {
  error err = exit_reason::normal;
  scheduled_actor::default_error_handler(ptr, err);
  return reflect(ptr, msg);
}

skippable_result print_and_drop(scheduled_actor* ptr, message& msg) {
  CAF_LOG_WARNING("unexpected message:" << msg);
  aout(ptr) << "*** unexpected message [id: " << ptr->id()
            << ", name: " << ptr->name() << "]: " << to_string(msg)
            << std::endl;
  return make_error(sec::unexpected_message);
}

skippable_result drop(scheduled_actor*, message&) {
  return make_error(sec::unexpected_message);
}

// -- implementation details ---------------------------------------------------

namespace {

template <class T>
void silently_ignore(scheduled_actor*, T&) {
  // nop
}

skippable_result drop_after_quit(scheduled_actor* self, message&) {
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
            << ", name: " << ptr->name() << "]: " << deep_to_string(x)
            << std::endl;
}

void scheduled_actor::default_node_down_handler(scheduled_actor* ptr,
                                                node_down_msg& x) {
  aout(ptr) << "*** unhandled node down message [id: " << ptr->id()
            << ", name: " << ptr->name() << "]: " << deep_to_string(x)
            << std::endl;
}

void scheduled_actor::default_exit_handler(scheduled_actor* ptr, exit_msg& x) {
  if (x.reason)
    default_error_handler(ptr, x.reason);
}

#ifdef CAF_ENABLE_EXCEPTIONS
error scheduled_actor::default_exception_handler(local_actor* ptr,
                                                 std::exception_ptr& x) {
  CAF_ASSERT(x != nullptr);
  try {
    std::rethrow_exception(x);
  } catch (std::exception& e) {
    auto pretty_type = detail::pretty_type_name(typeid(e));
    aout(ptr) << "*** unhandled exception: [id: " << ptr->id()
              << ", name: " << ptr->name()
              << ", exception typeid: " << pretty_type << "]: " << e.what()
              << std::endl;
    return make_error(sec::runtime_error, std::move(pretty_type), e.what());
  } catch (...) {
    aout(ptr) << "*** unhandled exception: [id: " << ptr->id()
              << ", name: " << ptr->name() << "]: unknown exception"
              << std::endl;
    return sec::runtime_error;
  }
}
#endif // CAF_ENABLE_EXCEPTIONS

// -- constructors and destructors ---------------------------------------------

scheduled_actor::scheduled_actor(actor_config& cfg)
  : super(cfg),
    mailbox_(unit, unit, unit),
    default_handler_(print_and_drop),
    error_handler_(default_error_handler),
    down_handler_(default_down_handler),
    node_down_handler_(default_node_down_handler),
    exit_handler_(default_exit_handler),
    private_thread_(nullptr)
#ifdef CAF_ENABLE_EXCEPTIONS
    ,
    exception_handler_(default_exception_handler)
#endif // CAF_ENABLE_EXCEPTIONS
{
  // nop
}

scheduled_actor::~scheduled_actor() {
  // nop
}

// -- overridden functions of abstract_actor -----------------------------------

bool scheduled_actor::enqueue(mailbox_element_ptr ptr, execution_unit* eu) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(!getf(is_blocking_flag));
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  CAF_LOG_SEND_EVENT(ptr);
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  auto collects_metrics = getf(abstract_actor::collects_metrics_flag);
  if (collects_metrics) {
    ptr->set_enqueue_time();
    metrics_.mailbox_size->inc();
  }
  switch (mailbox().push_back(std::move(ptr))) {
    case intrusive::inbox_result::unblocked_reader: {
      CAF_LOG_ACCEPT_EVENT(true);
      intrusive_ptr_add_ref(ctrl());
      if (private_thread_)
        private_thread_->resume(this);
      else if (eu != nullptr)
        eu->exec_later(this);
      else
        home_system().scheduler().enqueue(this);
      return true;
    }
    case intrusive::inbox_result::success:
      // enqueued to a running actors' mailbox; nothing to do
      CAF_LOG_ACCEPT_EVENT(false);
      return true;
    default: { // intrusive::inbox_result::queue_closed
      CAF_LOG_REJECT_EVENT();
      home_system().base_metrics().rejected_messages->inc();
      if (collects_metrics)
        metrics_.mailbox_size->dec();
      if (mid.is_request()) {
        detail::sync_request_bouncer f{exit_reason()};
        f(sender, mid);
      }
      return false;
    }
  }
}

mailbox_element* scheduled_actor::peek_at_next_mailbox_element() {
  if (mailbox().closed() || mailbox().blocked()) {
    return nullptr;
  } else if (awaited_responses_.empty()) {
    return mailbox().peek();
  } else {
    auto mid = awaited_responses_.begin()->first;
    auto pred = [mid](mailbox_element& x) { return x.mid == mid; };
    return mailbox().find_if(pred);
  }
}

// -- overridden functions of local_actor --------------------------------------

const char* scheduled_actor::name() const {
  return "user.scheduled-actor";
}

void scheduled_actor::launch(execution_unit* ctx, bool lazy, bool hide) {
  CAF_ASSERT(ctx != nullptr);
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
  CAF_ASSERT(!getf(is_blocking_flag));
  if (!hide)
    register_at_system();
  auto delay_first_scheduling = lazy && mailbox().try_block();
  if (getf(is_detached_flag)) {
    private_thread_ = ctx->system().acquire_private_thread();
    if (!delay_first_scheduling) {
      intrusive_ptr_add_ref(ctrl());
      private_thread_->resume(this);
    }
  } else if (!delay_first_scheduling) {
    intrusive_ptr_add_ref(ctrl());
    ctx->exec_later(this);
  }
}

bool scheduled_actor::cleanup(error&& fail_state, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(fail_state));
  pending_timeout_.dispose();
  // Shutdown hosting thread when running detached.
  if (private_thread_)
    home_system().release_private_thread(private_thread_);
  // Clear state for open requests.
  awaited_responses_.clear();
  multiplexed_responses_.clear();
  // Cancel any active flow.
  while (!watched_disposables_.empty()) {
    CAF_LOG_DEBUG("clean up" << watched_disposables_.size()
                             << "remaining disposables");
    for (auto& ptr : watched_disposables_)
      ptr.dispose();
    watched_disposables_.clear();
    run_actions();
  }
  // Clear mailbox.
  if (!mailbox_.closed()) {
    mailbox_.close();
    get_normal_queue().flush_cache();
    get_urgent_queue().flush_cache();
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

resumable::resume_result scheduled_actor::resume(execution_unit* ctx,
                                                 size_t max_throughput) {
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE(CAF_ARG(max_throughput));
  if (!activate(ctx))
    return resumable::done;
  size_t consumed = 0;
  auto reset_timeouts_if_needed = [&] {
    // Set a new receive timeout if we called our behavior at least once.
    if (consumed > 0)
      set_receive_timeout();
  };
  // Callback for handling urgent and normal messages.
  auto handle_async = [this, max_throughput, &consumed](mailbox_element& x) {
    return run_with_metrics(x, [this, max_throughput, &consumed, &x] {
      switch (reactivate(x)) {
        case activation_result::terminated:
          return intrusive::task_result::stop;
        case activation_result::success:
          return ++consumed < max_throughput ? intrusive::task_result::resume
                                             : intrusive::task_result::stop_all;
        case activation_result::skipped:
          return intrusive::task_result::skip;
        default:
          return intrusive::task_result::resume;
      }
    });
  };
  mailbox_element_ptr ptr;
  while (consumed < max_throughput) {
    CAF_LOG_DEBUG("start new DRR round");
    mailbox_.fetch_more();
    auto prev = consumed; // Caches the value before processing more.
    // TODO: maybe replace '3' with configurable / adaptive value?
    static constexpr size_t quantum = 3;
    // Dispatch urgent and normal (asynchronous) messages.
    get_urgent_queue().new_round(quantum * 3, handle_async);
    get_normal_queue().new_round(quantum, handle_async);
    // Update metrics or try returning if the actor consumed nothing.
    auto delta = consumed - prev;
    CAF_LOG_DEBUG("consumed" << delta << "messages this round");
    if (delta > 0) {
      auto signed_val = static_cast<int64_t>(delta);
      home_system().base_metrics().processed_messages->inc(signed_val);
    } else {
      reset_timeouts_if_needed();
      if (mailbox().try_block())
        return resumable::awaiting_message;
      CAF_LOG_DEBUG("mailbox().try_block() returned false");
    }
    CAF_LOG_DEBUG("check for shutdown");
    if (finalize())
      return resumable::done;
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
  if (getf(is_shutting_down_flag)) {
    CAF_LOG_DEBUG("already shutting down");
    return;
  }
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
  // Cancel any active flow.
  while (!watched_disposables_.empty()) {
    for (auto& ptr : watched_disposables_)
      ptr.dispose();
    watched_disposables_.clear();
    run_actions();
  }
}

// -- timeout management -------------------------------------------------------

void scheduled_actor::set_receive_timeout() {
  CAF_LOG_TRACE("");
  pending_timeout_.dispose();
  if (bhvr_stack_.empty()) {
    // nop
  } else if (auto delay = bhvr_stack_.back().timeout(); delay == infinite) {
    // nop
  } else {
    pending_timeout_ = run_delayed(delay, [this] {
      if (!bhvr_stack_.empty())
        bhvr_stack_.back().handle_timeout();
    });
  }
}

// -- caf::flow API ------------------------------------------------------------

flow::coordinator::steady_time_point scheduled_actor::steady_time() {
  return clock().now();
}

void scheduled_actor::ref_coordinator() const noexcept {
  intrusive_ptr_add_ref(ctrl());
}

void scheduled_actor::deref_coordinator() const noexcept {
  intrusive_ptr_release(ctrl());
}

void scheduled_actor::schedule(action what) {
  enqueue(nullptr, make_message_id(), make_message(std::move(what)), nullptr);
}

void scheduled_actor::delay(action what) {
  actions_.emplace_back(std::move(what));
}

disposable scheduled_actor::delay_until(steady_time_point abs_time,
                                        action what) {
  return clock().schedule(abs_time, std::move(what), strong_actor_ptr{ctrl()});
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
  if (auto view = make_typed_message_view<exit_msg>(content)) {
    auto& em = get<0>(view);
    // make sure to get rid of attachables if they're no longer needed
    unlink_from(em.source);
    // exit_reason::kill is always fatal
    if (em.reason == exit_reason::kill) {
      quit(std::move(em.reason));
    } else {
      call_handler(exit_handler_, this, em);
    }
    return message_category::internal;
  }
  if (auto view = make_typed_message_view<down_msg>(content)) {
    auto& dm = get<0>(view);
    call_handler(down_handler_, this, dm);
    return message_category::internal;
  }
  if (auto view = make_typed_message_view<action>(content)) {
    auto ptr = get<0>(view).ptr();
    CAF_ASSERT(ptr != nullptr);
    CAF_LOG_DEBUG("run action");
    ptr->run();
    return message_category::internal;
  }
  if (auto view = make_typed_message_view<node_down_msg>(content)) {
    auto& dm = get<0>(view);
    call_handler(node_down_handler_, this, dm);
    return message_category::internal;
  }
  if (auto view = make_typed_message_view<error>(content)) {
    auto& err = get<0>(view);
    call_handler(error_handler_, this, err);
    return message_category::internal;
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
      return f(in.content()) != std::nullopt;
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
          make_error(sec::unexpected_response, std::move(x.payload)));
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
        CAF_LOG_DEBUG("got unexpected_response");
        auto msg = make_message(
          make_error(sec::unexpected_response, std::move(x.payload)));
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
        if (!bhvr_stack_.empty()) {
          auto& bhvr = bhvr_stack_.back();
          if (bhvr(visitor, x.content()))
            return invoke_message_result::consumed;
        }
        auto sres = call_handler(default_handler_, this, x.payload);
        auto f = detail::make_overload(
          [&](auto& x) {
            visitor(x);
            return invoke_message_result::consumed;
          },
          [&](skip_t&) { return invoke_message_result::skipped; });
        return visit(f, sres);
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
#ifdef CAF_ENABLE_EXCEPTIONS
  try {
#endif // CAF_ENABLE_EXCEPTIONS
    if (!getf(is_initialized_flag)) {
      initialize();
      if (finalize()) {
        CAF_LOG_DEBUG("finalize() returned true right after make_behavior()");
        return false;
      }
      CAF_LOG_DEBUG("initialized actor:" << CAF_ARG(name()));
    }
#ifdef CAF_ENABLE_EXCEPTIONS
  } catch (...) {
    CAF_LOG_ERROR("actor died during initialization");
    auto eptr = std::current_exception();
    quit(call_handler(exception_handler_, this, eptr));
    finalize();
    return false;
  }
#endif // CAF_ENABLE_EXCEPTIONS
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
#ifdef CAF_ENABLE_EXCEPTIONS
  auto handle_exception = [&](std::exception_ptr eptr) {
    auto err = call_handler(exception_handler_, this, eptr);
    if (x.mid.is_request()) {
      auto rp = make_response_promise();
      rp.deliver(err);
    }
    quit(std::move(err));
  };
  try {
#endif // CAF_ENABLE_EXCEPTIONS
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
#ifdef CAF_ENABLE_EXCEPTIONS
  } catch (std::exception& e) {
    CAF_LOG_INFO("actor died because of an exception, what: " << e.what());
    static_cast<void>(e); // keep compiler happy when not logging
    handle_exception(std::current_exception());
  } catch (...) {
    CAF_LOG_INFO("actor died because of an unknown exception");
    handle_exception(std::current_exception());
  }
  finalize();
  return activation_result::terminated;
#endif // CAF_ENABLE_EXCEPTIONS
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
  // An actor is considered alive as long as it has a behavior, didn't set
  // the terminated flag and has no watched flows remaining.
  run_actions();
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
  auto push = [&ptr](auto& q) {
    q.inc_total_task_size(q.policy().task_size(*ptr));
    q.cache().push_back(ptr.release());
  };
  if (p.id_of(*ptr) == normal_queue_index)
    push(std::get<normal_queue_index>(qs));
  else
    push(std::get<urgent_queue_index>(qs));
}

scheduled_actor::urgent_queue& scheduled_actor::get_urgent_queue() {
  return get<urgent_queue_index>(mailbox_.queue().queues());
}

scheduled_actor::normal_queue& scheduled_actor::get_normal_queue() {
  return get<normal_queue_index>(mailbox_.queue().queues());
}

disposable scheduled_actor::run_scheduled(timestamp when, action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  CAF_LOG_TRACE(CAF_ARG(when));
  auto delay = when - make_timestamp();
  return run_scheduled(clock().now() + delay, std::move(what));
}

disposable scheduled_actor::run_scheduled(actor_clock::time_point when,
                                          action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  CAF_LOG_TRACE(CAF_ARG(when));
  return clock().schedule(when, std::move(what), strong_actor_ptr{ctrl()});
}

disposable scheduled_actor::run_delayed(timespan delay, action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  CAF_LOG_TRACE(CAF_ARG(delay));
  return clock().schedule(clock().now() + delay, std::move(what),
                          strong_actor_ptr{ctrl()});
}

// -- scheduling of caf::flow events -------------------------------------------

void scheduled_actor::watch(disposable obj) {
  CAF_ASSERT(obj.valid());
  watched_disposables_.emplace_back(std::move(obj));
  CAF_LOG_DEBUG("now watching" << watched_disposables_.size() << "disposables");
}

void scheduled_actor::run_actions() {
  if (!actions_.empty()) {
    // Note: can't use iterators here since actions may add to the vector.
    for (auto index = size_t{0}; index < actions_.size(); ++index) {
      auto f = std::move(actions_[index]);
      f.run();
    }
    actions_.clear();
  }
  update_watched_disposables();
}

void scheduled_actor::update_watched_disposables() {
  CAF_LOG_TRACE("");
  auto disposed = [](auto& hdl) { return hdl.disposed(); };
  auto& xs = watched_disposables_;
  if (auto e = std::remove_if(xs.begin(), xs.end(), disposed); e != xs.end()) {
    xs.erase(e, xs.end());
    CAF_LOG_DEBUG("now watching" << xs.size() << "disposables");
  }
}

} // namespace caf
