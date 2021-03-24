// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scheduled_actor.hpp"

#include "caf/actor_ostream.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/inbound_path.hpp"
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
    mailbox_(unit, unit, unit, unit, unit),
    timeout_id_(0),
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
  auto& sys_cfg = home_system().config();
  max_batch_delay_ = get_or(sys_cfg, "caf.stream.max_batch_delay",
                            defaults::stream::max_batch_delay);
}

scheduled_actor::~scheduled_actor() {
  // nop
}

// -- overridden functions of abstract_actor -----------------------------------

void scheduled_actor::enqueue(mailbox_element_ptr ptr, execution_unit* eu) {
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
      break;
    }
    case intrusive::inbox_result::queue_closed: {
      CAF_LOG_REJECT_EVENT();
      home_system().base_metrics().rejected_messages->inc();
      if (collects_metrics)
        metrics_.mailbox_size->dec();
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
  // Shutdown hosting thread when running detached.
  if (private_thread_)
    home_system().release_private_thread(private_thread_);
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
  actor_clock::time_point tout{actor_clock::duration_type{0}};
  auto reset_timeouts_if_needed = [&] {
    // Set a new receive timeout if we called our behavior at least once.
    if (consumed > 0)
      set_receive_timeout();
    // Set a new stream timeout.
    if (!stream_managers_.empty()) {
      // Make sure we call `advance_streams` at least once.
      if (tout.time_since_epoch().count() != 0)
        tout = advance_streams(clock().now());
      set_stream_timeout(tout);
    }
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
  // Callback for handling upstream messages (e.g., ACKs).
  auto handle_umsg = [this, max_throughput, &consumed](mailbox_element& x) {
    return run_with_metrics(x, [this, max_throughput, &consumed, &x] {
      current_mailbox_element(&x);
      CAF_LOG_RECEIVE_EVENT((&x));
      CAF_BEFORE_PROCESSING(this, x);
      CAF_ASSERT(x.content().match_elements<upstream_msg>());
      auto& um = x.content().get_mutable_as<upstream_msg>(0);
      auto f = [&](auto& content) {
        handle_upstream_msg(um.slots, um.sender, content);
      };
      visit(f, um.content);
      CAF_AFTER_PROCESSING(this, invoke_message_result::consumed);
      return ++consumed < max_throughput ? intrusive::task_result::resume
                                         : intrusive::task_result::stop_all;
    });
  };
  // Callback for handling downstream messages (e.g., batches).
  auto handle_dmsg = [this, &consumed, max_throughput](stream_slot, auto& q,
                                                       mailbox_element& x) {
    return run_with_metrics(x, [this, max_throughput, &consumed, &q, &x] {
      current_mailbox_element(&x);
      CAF_LOG_RECEIVE_EVENT((&x));
      CAF_BEFORE_PROCESSING(this, x);
      CAF_ASSERT(x.content().match_elements<downstream_msg>());
      auto& dm = x.content().get_mutable_as<downstream_msg>(0);
      auto f = [&, this](auto& content) {
        using content_type = std::decay_t<decltype(content)>;
        auto& inptr = q.policy().handler;
        if (inptr == nullptr)
          return intrusive::task_result::stop;
        if (auto processed_elements = inptr->metrics.processed_elements) {
          auto num_elements = q.policy().task_size(content);
          auto input_buffer_size = inptr->metrics.input_buffer_size;
          CAF_ASSERT(input_buffer_size != nullptr);
          processed_elements->inc(num_elements);
          input_buffer_size->dec(num_elements);
        }
        // Hold onto a strong reference since we might reset `inptr`.
        auto mgr = stream_manager_ptr{inptr->mgr};
        inptr->handle(content);
        // The sender slot can be 0. This is the case for forced_close or
        // forced_drop messages from stream aborters.
        CAF_ASSERT(inptr->slots == dm.slots
                   || (dm.slots.sender == 0
                       && dm.slots.receiver == inptr->slots.receiver));
        if constexpr (std::is_same<content_type, downstream_msg::close>::value
                      || std::is_same<content_type,
                                      downstream_msg::forced_close>::value) {
          if (auto input_buffer_size = inptr->metrics.input_buffer_size)
            input_buffer_size->dec(q.total_task_size());
          inptr.reset();
          get_downstream_queue().erase_later(dm.slots.receiver);
          erase_stream_manager(dm.slots.receiver);
          if (mgr->done()) {
            CAF_LOG_DEBUG("path is done receiving and closes its manager");
            erase_stream_manager(mgr);
            mgr->stop();
          }
          return intrusive::task_result::stop;
        } else if (mgr->done()) {
          CAF_LOG_DEBUG("path is done receiving and closes its manager");
          erase_stream_manager(mgr);
          mgr->stop();
          return intrusive::task_result::stop;
        }
        return intrusive::task_result::resume;
      };
      auto res = visit(f, dm.content);
      CAF_AFTER_PROCESSING(this, invoke_message_result::consumed);
      return ++consumed < max_throughput ? res
                                         : intrusive::task_result::stop_all;
    });
  };
  std::vector<stream_manager*> managers;
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
    // Consume all upstream messages. They are lightweight by design and ACKs
    // come with new credit, allowing us to advance stream traffic.
    if (auto tts = get_upstream_queue().total_task_size(); tts > 0) {
      get_upstream_queue().new_round(tts, handle_umsg);
      // After processing ACKs, we may have new credit that enables us to ship
      // some batches from our output buffers. This step also may re-enable
      // inbound paths by draining output buffers here.
      active_stream_managers(managers);
      for (auto mgr : managers)
        mgr->push();
    }
    // Note: a quantum of 1 means "1 batch" for this queue.
    if (get_downstream_queue().new_round(quantum, handle_dmsg).consumed_items
        > 0) {
      do {
        // Processing batches, enables stages to push more data downstream. This
        // in turn may allow the stage to process more batches again. Hence the
        // loop. By not giving additional quanta, we simply allow the stage to
        // consume what it was allowed to in the first place.
        active_stream_managers(managers);
        for (auto mgr : managers)
          mgr->push();
      } while (
        consumed < max_throughput
        && get_downstream_queue().new_round(0, handle_dmsg).consumed_items > 0);
    }
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
    CAF_LOG_DEBUG("allow stream managers to send batches");
    active_stream_managers(managers);
    for (auto mgr : managers)
      mgr->push();
    CAF_LOG_DEBUG("check for shutdown or advance streams");
    if (finalize())
      return resumable::done;
    if (auto now = clock().now(); now >= tout)
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

// -- actor metrics ------------------------------------------------------------

auto scheduled_actor::inbound_stream_metrics(type_id_t type)
  -> inbound_stream_metrics_t {
  if (!has_metrics_enabled())
    return {nullptr, nullptr};
  if (auto i = inbound_stream_metrics_.find(type);
      i != inbound_stream_metrics_.end())
    return i->second;
  auto actor_name_cstr = name();
  auto actor_name = string_view{actor_name_cstr, strlen(actor_name_cstr)};
  auto tname = query_type_name(type);
  auto fs = system().actor_metric_families().stream;
  inbound_stream_metrics_t result{
    fs.processed_elements->get_or_add({{"name", actor_name}, {"type", tname}}),
    fs.input_buffer_size->get_or_add({{"name", actor_name}, {"type", tname}}),
  };
  inbound_stream_metrics_.emplace(type, result);
  return result;
}

auto scheduled_actor::outbound_stream_metrics(type_id_t type)
  -> outbound_stream_metrics_t {
  if (!has_metrics_enabled())
    return {nullptr, nullptr};
  if (auto i = outbound_stream_metrics_.find(type);
      i != outbound_stream_metrics_.end())
    return i->second;
  auto actor_name_cstr = name();
  auto actor_name = string_view{actor_name_cstr, strlen(actor_name_cstr)};
  auto tname = query_type_name(type);
  auto fs = system().actor_metric_families().stream;
  outbound_stream_metrics_t result{
    fs.pushed_elements->get_or_add({{"name", actor_name}, {"type", tname}}),
    fs.output_buffer_size->get_or_add({{"name", actor_name}, {"type", tname}}),
  };
  outbound_stream_metrics_.emplace(type, result);
  return result;
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
    } else if (tm.type == "stream") {
      CAF_LOG_DEBUG("handle stream timeout message");
      set_stream_timeout(advance_streams(clock().now()));
    } else {
      // Drop. Other types not supported yet.
    }
    return message_category::internal;
  }
  if (auto view = make_typed_message_view<exit_msg>(content)) {
    auto& em = get<0>(view);
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
  if (auto view = make_typed_message_view<down_msg>(content)) {
    auto& dm = get<0>(view);
    call_handler(down_handler_, this, dm);
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
        auto had_timeout = getf(has_timeout_flag);
        if (had_timeout)
          unsetf(has_timeout_flag);
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
          [&](skip_t&) {
            if (had_timeout)
              setf(has_timeout_flag);
            return invoke_message_result::skipped;
          });
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
  // TODO: This is a workaround for issue #1011. Iterating over all stream
  //       managers here and dropping them as needed prevents the
  //       never-terminating part, but it still means that "dead" stream manager
  //       can accumulate over time since we only run this O(n) path if the
  //       actor is shutting down.
  if (!has_behavior() && !stream_managers_.empty()) {
    for (auto i = stream_managers_.begin(); i != stream_managers_.end();) {
      if (i->second->done())
        i = stream_managers_.erase(i);
      else
        ++i;
    }
  }
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

scheduled_actor::upstream_queue& scheduled_actor::get_upstream_queue() {
  return get<upstream_queue_index>(mailbox_.queue().queues());
}

scheduled_actor::downstream_queue& scheduled_actor::get_downstream_queue() {
  return get<downstream_queue_index>(mailbox_.queue().queues());
}

bool scheduled_actor::add_inbound_path(type_id_t,
                                       std::unique_ptr<inbound_path> path) {
  static constexpr size_t queue_index = downstream_queue_index;
  using policy_type = policy::downstream_messages::nested;
  auto& qs = get<queue_index>(mailbox_.queue().queues()).queues();
  auto res = qs.emplace(path->slots.receiver, policy_type{nullptr});
  if (!res.second)
    return false;
  res.first->second.policy().handler = std::move(path);
  return true;
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
  if (auto [moved, ptr] = ack_pending_stream_manager(slots.receiver); moved) {
    CAF_ASSERT(ptr != nullptr);
    ptr->handle(slots, x);
  } else {
    CAF_LOG_WARNING("found no corresponding manager for received ack_open");
  }
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
    auto highest = x.rbegin()->first;
    if (highest < std::numeric_limits<decltype(highest)>::max())
      return highest + 1;
    // Back-up algorithm in the case of an overflow: Take the minimum
    // id `1` if its still free, otherwise take an id from the first
    // gap between two consecutive keys.
    if (1 < x.begin()->first)
      return 1;
    auto has_gap = [](auto& a, auto& b) { return b.first - a.first > 1; };
    auto i = std::adjacent_find(x.begin(), x.end(), has_gap);
    return i != x.end() ? i->first + 1 : invalid_stream_slot;
  };
  if (!stream_managers_.empty())
    result = std::max(nslot(stream_managers_), result);
  if (!pending_stream_managers_.empty())
    result = std::max(nslot(pending_stream_managers_), result);
  return result;
}

void scheduled_actor::assign_slot(stream_slot x, stream_manager_ptr mgr) {
  CAF_LOG_TRACE(CAF_ARG(x));
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
  return stream_managers_.emplace(id, std::move(mgr)).second;
}

void scheduled_actor::erase_stream_manager(stream_slot id) {
  CAF_LOG_TRACE(CAF_ARG(id));
  stream_managers_.erase(id);
  CAF_LOG_DEBUG(CAF_ARG2("stream_managers_.size", stream_managers_.size()));
}

void scheduled_actor::erase_pending_stream_manager(stream_slot id) {
  CAF_LOG_TRACE(CAF_ARG(id));
  pending_stream_managers_.erase(id);
}

std::pair<bool, stream_manager*>
scheduled_actor::ack_pending_stream_manager(stream_slot id) {
  CAF_LOG_TRACE(CAF_ARG(id));
  if (auto i = pending_stream_managers_.find(id);
      i != pending_stream_managers_.end()) {
    auto ptr = std::move(i->second);
    auto raw_ptr = ptr.get();
    pending_stream_managers_.erase(i);
    return {add_stream_manager(id, std::move(ptr)), raw_ptr};
  }
  return {false, nullptr};
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
    void operator()(error&) override {
      // nop
    }

    void operator()(message&) override {
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
  // Invoke behavior and dispatch on the result.
  auto& bs = bhvr_stack();
  if (!bs.empty() && bs.back()(f, osm.msg))
    return invoke_message_result::consumed;
  CAF_LOG_DEBUG("no match in behavior, fall back to default handler");
  auto sres = call_handler(default_handler_, this, x.payload);
  if (holds_alternative<skip_t>(sres)) {
    CAF_LOG_DEBUG("default handler skipped open_stream_msg:" << osm.msg);
    return invoke_message_result::skipped;
  } else {
    CAF_LOG_DEBUG("default handler was called for open_stream_msg:" << osm.msg);
    fail(sec::stream_init_failed, "dropped open_stream_msg (no match)");
    return invoke_message_result::dropped;
  }
}

actor_clock::time_point
scheduled_actor::advance_streams(actor_clock::time_point now) {
  CAF_LOG_TRACE(CAF_ARG(now));
  if (stream_managers_.empty())
    return actor_clock::time_point::max();
  auto managers = active_stream_managers();
  for (auto ptr : managers)
    ptr->tick(now);
  auto idle = [](const stream_manager* mgr) { return mgr->idle(); };
  if (std::all_of(managers.begin(), managers.end(), idle))
    return actor_clock::time_point::max();
  return now + max_batch_delay_;
}

void scheduled_actor::active_stream_managers(std::vector<stream_manager*>& xs) {
  xs.clear();
  if (stream_managers_.empty())
    return;
  xs.reserve(stream_managers_.size());
  for (auto& kvp : stream_managers_)
    xs.emplace_back(kvp.second.get());
  std::sort(xs.begin(), xs.end());
  auto e = std::unique(xs.begin(), xs.end());
  xs.erase(e, xs.end());
}

std::vector<stream_manager*> scheduled_actor::active_stream_managers() {
  std::vector<stream_manager*> result;
  active_stream_managers(result);
  return result;
}

} // namespace caf
