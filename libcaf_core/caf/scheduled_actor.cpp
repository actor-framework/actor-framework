// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/scheduled_actor.hpp"

#include "caf/action.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/anon_mail.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/mcast.hpp"
#include "caf/format_to_error.hpp"
#include "caf/log/core.hpp"
#include "caf/log/system.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/scheduler.hpp"
#include "caf/send.hpp"
#include "caf/stream.hpp"

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
  log::system::warning("discarded unexpected message (id: {}, name: {}): {}",
                       ptr->id(), ptr->name(), msg);
  ptr->println("*** unexpected message [id: {}, name: {}]: {}", ptr->id(),
               ptr->name(), msg);
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
  ptr->println("*** unhandled down message [id: {}, name: {}]: {}", ptr->id(),
               ptr->name(), x);
}

void scheduled_actor::default_node_down_handler(scheduled_actor* ptr,
                                                node_down_msg& x) {
  ptr->println("*** unhandled node down message [id: {} , name: {}]: {}",
               ptr->id(), ptr->name(), x);
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
    ptr->println(
      "*** unhandled exception: [id: {}, name: {}, exception: {}]: {}",
      ptr->id(), ptr->name(), pretty_type, e.what());
    return format_to_error(sec::runtime_error,
                           "unhandled exception of type {}: {}", pretty_type,
                           e.what());
  } catch (...) {
    ptr->println(
      "*** unhandled exception: [id: {}, name: {}]: unknown exception",
      ptr->id(), ptr->name());
    return make_error(sec::runtime_error,
                      "unhandled exception of unknown type");
  }
}
#endif // CAF_ENABLE_EXCEPTIONS

// -- constructors and destructors ---------------------------------------------

scheduled_actor::batch_forwarder::~batch_forwarder() {
  // nop
}

scheduled_actor::scheduled_actor(actor_config& cfg)
  : super(cfg),
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
  if (cfg.mbox_factory == nullptr)
    mailbox_ = new (&default_mailbox_) detail::default_mailbox();
  else
    mailbox_ = cfg.mbox_factory->make(this);
}

scheduled_actor::~scheduled_actor() {
  unstash();
  if (mailbox_ == &default_mailbox_)
    default_mailbox_.~default_mailbox();
  else
    mailbox_->deref_mailbox();
}

// -- overridden functions of abstract_actor -----------------------------------

bool scheduled_actor::enqueue(mailbox_element_ptr ptr, scheduler* sched) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(!getf(is_blocking_flag));
  auto lg = log::core::trace("ptr = {}", *ptr);
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
      else if (sched != nullptr)
        sched->delay(this);
      else
        home_system().scheduler().schedule(this);
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

// -- overridden functions of local_actor --------------------------------------

const char* scheduled_actor::name() const {
  return "user.scheduled-actor";
}

void scheduled_actor::launch(scheduler* sched, bool lazy, bool hide) {
  CAF_ASSERT(sched != nullptr);
  CAF_PUSH_AID_FROM_PTR(this);
  auto lg = log::core::trace("lazy = {}, hide = {}", lazy, hide);
  CAF_ASSERT(!getf(is_blocking_flag));
  if (!hide)
    register_at_system();
  auto delay_first_scheduling = lazy && mailbox().try_block();
  if (getf(is_detached_flag)) {
    private_thread_ = system().acquire_private_thread();
    if (!delay_first_scheduling) {
      intrusive_ptr_add_ref(ctrl());
      private_thread_->resume(this);
    }
  } else if (!delay_first_scheduling) {
    intrusive_ptr_add_ref(ctrl());
    sched->delay(this);
  }
}

void scheduled_actor::on_cleanup(const error& reason) {
  auto lg = log::core::trace("reason = {}", reason);
  timeout_state_.pending.dispose();
  // Shutdown hosting thread when running detached.
  if (private_thread_)
    home_system().release_private_thread(private_thread_);
  // Clear state for open requests, flows and streams.
  awaited_responses_.clear();
  multiplexed_responses_.clear();
  cancel_flows_and_streams();
  close_mailbox(reason);
  // Dispatch to parent's `on_cleanup` function.
  super::on_cleanup(reason);
}

// -- overridden functions of resumable ----------------------------------------

resumable::subtype_t scheduled_actor::subtype() const noexcept {
  return resumable::scheduled_actor;
}

void scheduled_actor::ref_resumable() const noexcept {
  intrusive_ptr_add_ref(ctrl());
}

void scheduled_actor::deref_resumable() const noexcept {
  intrusive_ptr_release(ctrl());
}

resumable::resume_result scheduled_actor::resume(scheduler* sched,
                                                 size_t max_throughput) {
  CAF_PUSH_AID(id());
  auto lg = log::core::trace("max_throughput = {}", max_throughput);
  if (!activate(sched))
    return resumable::done;
  size_t consumed = 0;
  auto guard = detail::scope_guard{[this, &consumed]() noexcept {
    if (consumed > 0) {
      auto val = static_cast<int64_t>(consumed);
      home_system().base_metrics().processed_messages->inc(val);
    }
  }};
  auto reset_timeouts_if_needed = [&] {
    // Set a new receive timeout if we called our behavior at least once.
    if (consumed > 0)
      set_receive_timeout();
  };
  mailbox_element_ptr ptr;
  while (consumed < max_throughput) {
    auto ptr = mailbox().pop_front();
    if (!ptr) {
      if (mailbox().try_block()) {
        reset_timeouts_if_needed();
        log::core::debug("mailbox empty: await new messages");
        return resumable::awaiting_message;
      }
      continue; // Interrupted by a new message, try again.
    }
    auto res = run_with_metrics(*ptr, [this, &ptr, &consumed] {
      auto res = reactivate(*ptr);
      switch (res) {
        case activation_result::success:
          ++consumed;
          unstash();
          break;
        case activation_result::skipped:
          stash_.push(ptr.release());
          break;
        default: // drop
          break;
      }
      return res;
    });
    if (res == activation_result::terminated)
      return resumable::done;
  }
  reset_timeouts_if_needed();
  if (mailbox().try_block()) {
    log::core::debug("mailbox empty: await new messages");
    return resumable::awaiting_message;
  }
  // time's up
  log::core::debug("max throughput reached: resume later");
  return resumable::resume_later;
}

// -- scheduler callbacks ------------------------------------------------------

proxy_registry* scheduled_actor::proxy_registry_ptr() {
  return nullptr;
}

// -- state modifiers ----------------------------------------------------------

void scheduled_actor::quit(error x) {
  auto lg = log::core::trace("x = {}", x);
  // Make sure repeated calls to quit don't do anything.
  if (getf(is_shutting_down_flag)) {
    log::core::debug("already shutting down");
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
  exit_handler_ = silently_ignore<exit_msg>;
  down_handler_ = silently_ignore<down_msg>;
  error_handler_ = silently_ignore<error>;
  // Drop future messages and produce sec::request_receiver_down for requests.
  default_handler_ = drop_after_quit;
  // Make sure we're not waiting for flows or stream anymore.
  cancel_flows_and_streams();
}

// -- timeout management -------------------------------------------------------

void scheduled_actor::set_receive_timeout() {
  auto lg = log::core::trace("");
  timeout_state_.pending.dispose();
  if (timeout_state_.delay == infinite)
    return;
  switch (timeout_state_.mode) {
    default:
      break;
    case timeout_mode::once_weak:
    case timeout_mode::repeat_weak:
      timeout_state_.id = new_u64_id();
      timeout_state_.pending = clock().schedule_message(
        nullptr, weak_actor_ptr{ctrl()}, clock().now() + timeout_state_.delay,
        make_message_id(), make_message(timeout_msg{timeout_state_.id}));
      break;
    case timeout_mode::legacy:
      if (bhvr_stack_.empty()) {
        timeout_state_.reset();
        return;
      }
      [[fallthrough]];
    case timeout_mode::once_strong:
    case timeout_mode::repeat_strong:
      timeout_state_.id = new_u64_id();
      timeout_state_.pending = clock().schedule_message(
        nullptr, strong_actor_ptr{ctrl()}, clock().now() + timeout_state_.delay,
        make_message_id(), make_message(timeout_msg{timeout_state_.id}));
      break;
  }
}

void scheduled_actor::handle_timeout() {
  switch (timeout_state_.mode) {
    default:
      log::core::error("invalid timeout mode");
      break;
    case timeout_mode::once_weak:
    case timeout_mode::once_strong: {
      if (!timeout_state_.handler) {
        log::core::error("received a timeout but no handler was set");
        break;
      }
      auto f = std::move(timeout_state_.handler);
      timeout_state_.reset();
      f();
      break;
    }
    case timeout_mode::repeat_weak:
    case timeout_mode::repeat_strong: {
      timeout_state_.pending = nullptr; // Discard obsolete timeout.
      timeout_state st;
      st.swap(timeout_state_);
      if (!st.handler) {
        log::core::error("received a timeout but no handler was set");
        break;
      }
      st.handler();
      if (timeout_state_.mode != timeout_mode::none) {
        log::core::debug("timeout handler called set_idle_handler");
        break;
      }
      timeout_state_.swap(st);
      set_receive_timeout();
      break;
    }
    case timeout_mode::legacy: {
      timeout_state_.pending = nullptr; // Discard obsolete timeout.
      if (bhvr_stack_.empty()) {
        log::core::error("received a (legacy) timeout but no behavior was set");
        break;
      }
      bhvr_stack_.back().handle_timeout();
      set_receive_timeout();
      break;
    }
  }
}

// -- caf::flow API ------------------------------------------------------------

namespace detail {

// Forwards batches from a local flow to another actor.
class batch_forwarder_impl : public scheduled_actor::batch_forwarder,
                             public flow::observer_impl<async::batch> {
public:
  batch_forwarder_impl(scheduled_actor* self, actor sink_hdl,
                       uint64_t sink_flow_id, uint64_t source_flow_id)
    : self_(self),
      sink_hdl_(sink_hdl),
      sink_flow_id_(sink_flow_id),
      source_flow_id_(source_flow_id) {
    // nop
  }

  flow::coordinator* parent() const noexcept override {
    return self_;
  }

  void cancel() override {
    if (sink_hdl_) {
      // Note: must send this as anonymous message, because this can be called
      // from on_destroy().
      anon_mail(stream_abort_msg{sink_flow_id_, sec::stream_aborted})
        .send(sink_hdl_);
      sink_hdl_ = nullptr;
    }
    sub_.cancel();
  }

  void request(size_t num_items) override {
    if (sub_)
      sub_.request(num_items);
  }

  void ref_coordinated() const noexcept final {
    ref();
  }

  void deref_coordinated() const noexcept final {
    deref();
  }

  bool subscribed() const noexcept {
    return sub_.valid();
  }

  void on_next(const async::batch& content) override {
    unsafe_send_as(self_, sink_hdl_, stream_batch_msg{sink_flow_id_, content});
  }

  void on_error(const error& err) override {
    unsafe_send_as(self_, sink_hdl_, stream_abort_msg{sink_flow_id_, err});
    sink_hdl_ = nullptr;
    sub_.release_later();
    self_->stream_subs_.erase(source_flow_id_);
  }

  void on_complete() override {
    unsafe_send_as(self_, sink_hdl_, stream_close_msg{sink_flow_id_});
    sink_hdl_ = nullptr;
    sub_.release_later();
    self_->stream_subs_.erase(source_flow_id_);
  }

  void on_subscribe(flow::subscription sub) override {
    if (!sub_ && sink_hdl_)
      sub_ = sub;
    else
      sub.cancel();
  }

  friend void intrusive_ptr_add_ref(const batch_forwarder_impl* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const batch_forwarder_impl* ptr) noexcept {
    ptr->deref();
  }

private:
  scheduled_actor* self_;
  actor sink_hdl_;
  uint64_t sink_flow_id_;
  uint64_t source_flow_id_;
  flow::subscription sub_;
};

} // namespace detail

flow::coordinator::steady_time_point scheduled_actor::steady_time() {
  return clock().now();
}

void scheduled_actor::ref_execution_context() const noexcept {
  intrusive_ptr_add_ref(ctrl());
}

void scheduled_actor::deref_execution_context() const noexcept {
  intrusive_ptr_release(ctrl());
}

void scheduled_actor::schedule(action what) {
  enqueue(make_mailbox_element(nullptr, make_message_id(), std::move(what)),
          nullptr);
}

void scheduled_actor::delay(action what) {
  // Happy path: push it to `actions_`, where we run it from `run_actions`.
  if (delayed_actions_this_run_++ < defaults::max_inline_actions_per_run) {
    actions_.emplace_back(std::move(what));
    return;
  }
  // Slow path: we send a "request" with the action to ourselves. The pending
  // request makes sure that the action keeps the actor alive until processed.
  if (!delay_bhvr_) {
    delay_bhvr_ = behavior{[](action& f) {
      log::core::debug("run delayed action");
      f.run();
    }};
  }
  auto res_id = new_request_id(message_priority::normal).response_id();
  enqueue(make_mailbox_element(nullptr, res_id, std::move(what)), context_);
  add_multiplexed_response_handler(res_id, delay_bhvr_);
}

disposable scheduled_actor::delay_until(steady_time_point abs_time,
                                        action what) {
  return clock().schedule(abs_time, std::move(what), strong_actor_ptr{ctrl()});
}

// -- message processing -------------------------------------------------------

void scheduled_actor::add_awaited_response_handler(message_id response_id,
                                                   behavior bhvr,
                                                   disposable pending_timeout) {
  awaited_responses_.emplace_front(response_id, std::move(bhvr),
                                   std::move(pending_timeout));
}

void scheduled_actor::add_multiplexed_response_handler(
  message_id response_id, behavior bhvr, disposable pending_timeout) {
  multiplexed_responses_.emplace(
    response_id, std::pair{std::move(bhvr), std::move(pending_timeout)});
}

scheduled_actor::message_category
scheduled_actor::categorize(mailbox_element& x) {
  auto lg = log::core::trace("x = {}", x);
  auto& content = x.content();
  if (content.match_elements<sys_atom, get_atom, std::string>()) {
    auto rp = make_response_promise();
    if (!rp.pending()) {
      log::system::warning("received anonymous ('get', 'sys', $key) message");
      return message_category::internal;
    }
    auto& what = content.get_as<std::string>(2);
    if (what == "info") {
      log::core::debug("reply to 'info' message");
      rp.deliver(ok_atom_v, what, strong_actor_ptr{ctrl()}, name());
    } else {
      rp.deliver(make_error(sec::unsupported_sys_key));
    }
    return message_category::internal;
  }
  if (content.size() != 1)
    return message_category::ordinary;
  switch (content.type_at(0)) {
    case type_id_v<exit_msg>: {
      auto& em = x.payload.get_mutable_as<exit_msg>(0);
      // Make sure to get rid of attachables if they're no longer needed.
      unlink_from(em.source);
      // Receiving exit_reason::kill is always fatal
      if (em.reason == exit_reason::kill) {
        quit(std::move(em.reason));
        return message_category::internal;
      }
      // Handle non-kill exit messages in the behavior. If the behavior doesn't
      // handle the message, we will call the exit handler later.
      return message_category::ordinary;
    }
    case type_id_v<timeout_msg>: {
      auto id = content.get_as<timeout_msg>(0).id;
      if (timeout_state_.id == id)
        handle_timeout();
      return message_category::internal;
    }
    case type_id_v<action>: {
      auto what = content.get_as<action>(0);
      CAF_ASSERT(what.ptr() != nullptr);
      log::core::debug("run action");
      what.run();
      return message_category::internal;
    }
    case type_id_v<stream_open_msg>: {
      // Try to subscribe the sink to the observable.
      auto& [str_id, ptr, sink_id] = content.get_as<stream_open_msg>(0);
      if (!ptr) {
        log::system::error("received a stream_open_msg with a null sink");
        return message_category::internal;
      }
      auto sink_hdl = actor_cast<actor>(ptr);
      if (auto i = stream_sources_.find(str_id); i != stream_sources_.end()) {
        // Create a forwarder that turns observed items into batches.
        auto flow_id = new_u64_id();
        auto fwd = make_counted<detail::batch_forwarder_impl>(this, sink_hdl,
                                                              sink_id, flow_id);
        auto sub = i->second.obs->subscribe(flow::observer<async::batch>{fwd});
        if (fwd->subscribed()) {
          // Inform the sink that the stream is now open.
          stream_subs_.emplace(flow_id, std::move(fwd));
          auto mipb = static_cast<uint32_t>(i->second.max_items_per_batch);
          unsafe_send_as(this, sink_hdl,
                         stream_ack_msg{ctrl(), sink_id, flow_id, mipb});
          if (sink_hdl.node() != node()) {
            // Actors cancel any pending streams when they terminate. However,
            // remote actors may terminate without sending us a proper goodbye.
            // Hence, we add a function object to remote actors to make sure we
            // get a cancel in all cases.
            auto weak_self = weak_actor_ptr{ctrl()};
            sink_hdl->attach_functor([weak_self, flow_id] {
              if (auto sptr = weak_self.lock())
                caf::anon_mail(stream_cancel_msg{flow_id})
                  .send(actor_cast<actor>(sptr));
            });
          }
          return message_category::internal;
        }
        log::system::error("failed to subscribe a batch forwarder");
        sub.dispose();
      }
      // Abort the flow immediately.
      log::core::debug("requested stream does not exist");
      auto err = make_error(sec::invalid_stream);
      unsafe_send_as(this, sink_hdl, stream_abort_msg{sink_id, std::move(err)});
      return message_category::internal;
    }
    case type_id_v<stream_demand_msg>: {
      auto [sub_id, new_demand] = content.get_as<stream_demand_msg>(0);
      if (auto i = stream_subs_.find(sub_id); i != stream_subs_.end()) {
        // Note: `i` might become invalid as a result of calling `request`.
        auto ptr = i->second;
        ptr->request(new_demand);
      }
      return message_category::internal;
    }
    case type_id_v<stream_cancel_msg>: {
      auto [sub_id] = content.get_as<stream_cancel_msg>(0);
      if (auto i = stream_subs_.find(sub_id); i != stream_subs_.end()) {
        log::core::debug("canceled stream {}", sub_id);
        auto ptr = i->second;
        stream_subs_.erase(i);
        ptr->cancel();
      }
      return message_category::internal;
    }
    case type_id_v<stream_ack_msg>: {
      auto [ptr, sink_id, src_id, mipb] = content.get_as<stream_ack_msg>(0);
      if (auto i = stream_bridges_.find(sink_id); i != stream_bridges_.end()) {
        auto ptr = i->second;
        ptr->ack(src_id, mipb);
      }
      return message_category::internal;
    }
    case type_id_v<stream_batch_msg>: {
      const auto& [sink_id, xs] = content.get_as<stream_batch_msg>(0);
      if (auto i = stream_bridges_.find(sink_id); i != stream_bridges_.end()) {
        auto ptr = i->second;
        ptr->push(xs);
      }
      return message_category::internal;
    }
    case type_id_v<stream_close_msg>: {
      auto [sink_id] = content.get_as<stream_close_msg>(0);
      if (auto i = stream_bridges_.find(sink_id); i != stream_bridges_.end()) {
        auto ptr = i->second;
        stream_bridges_.erase(i);
        ptr->drop();
      }
      return message_category::internal;
    }
    case type_id_v<stream_abort_msg>: {
      const auto& [sink_id, reason] = content.get_as<stream_abort_msg>(0);
      if (auto i = stream_bridges_.find(sink_id); i != stream_bridges_.end()) {
        auto ptr = i->second;
        stream_bridges_.erase(i);
        ptr->drop(reason);
      }
      return message_category::internal;
    }
    default:
      return message_category::ordinary;
  }
}

invoke_message_result scheduled_actor::consume(mailbox_element& x) {
  auto lg = log::core::trace("x = {}", x);
  current_element_ = &x;
  CAF_LOG_RECEIVE_EVENT(current_element_);
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
      // Skip all other messages until we receive the currently awaited response
      // except for internal (system) messages.
      if (x.mid != std::get<0>(pr)) {
        // Responses are never internal messages and must be skipped here.
        // Otherwise, an error to a response would run into the default handler.
        if (x.mid.is_response())
          return invoke_message_result::skipped;
        if (categorize(x) == message_category::internal) {
          log::core::debug("handled system message");
          return invoke_message_result::consumed;
        }
        return invoke_message_result::skipped;
      }
      auto f = std::move(std::get<1>(pr));
      std::get<2>(pr).dispose(); // Stop the timeout.
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
      auto bhvr = std::move(mrh->second.first);
      mrh->second.second.dispose(); // Stop the timeout.
      multiplexed_responses_.erase(mrh);
      if (!invoke(this, bhvr, x)) {
        log::core::debug("got unexpected_response");
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
        log::core::debug("handled system message");
        return invoke_message_result::consumed;
      case message_category::ordinary: {
        detail::default_invoke_result_visitor<scheduled_actor> visitor{this};
        if (!bhvr_stack_.empty()) {
          auto& bhvr = bhvr_stack_.back();
          if (bhvr(visitor, x.payload))
            return invoke_message_result::consumed;
        }
        if (x.payload.size() == 1) {
          switch (x.payload.type_at(0)) {
            default:
              break;
            case type_id_v<exit_msg>: {
              auto& em = x.payload.get_mutable_as<exit_msg>(0);
              call_handler(exit_handler_, this, em);
              return invoke_message_result::consumed;
            }
            case type_id_v<down_msg>: {
              auto& dm = x.payload.get_mutable_as<down_msg>(0);
              call_handler(down_handler_, this, dm);
              return invoke_message_result::consumed;
            }
            case type_id_v<node_down_msg>: {
              auto& dm = x.payload.get_mutable_as<node_down_msg>(0);
              call_handler(node_down_handler_, this, dm);
              return invoke_message_result::consumed;
            }
            case type_id_v<error>: {
              auto& err = x.payload.get_mutable_as<error>(0);
              call_handler(error_handler_, this, err);
              return invoke_message_result::consumed;
            }
          }
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

bool scheduled_actor::activate(scheduler* sched) {
  auto lg = log::core::trace("");
  CAF_ASSERT(sched != nullptr);
  CAF_ASSERT(!getf(is_blocking_flag));
  context(sched);
  if (getf(is_initialized_flag) && !alive()) {
    log::system::warning("activate called on a terminated actor "
                         "with id {} and name {}",
                         id(), name());
    return false;
  }
#ifdef CAF_ENABLE_EXCEPTIONS
  try {
#endif // CAF_ENABLE_EXCEPTIONS
    if (!getf(is_initialized_flag)) {
      initialize();
      if (finalize()) {
        log::core::debug(
          "finalize() returned true right after make_behavior()");
        return false;
      }
      log::core::debug("initialized actor: name = {}", name());
    }
#ifdef CAF_ENABLE_EXCEPTIONS
  } catch (...) {
    log::core::debug("failed to initialize actor due to an exception");
    auto eptr = std::current_exception();
    quit(call_handler(exception_handler_, this, eptr));
    finalize();
    return false;
  }
#endif // CAF_ENABLE_EXCEPTIONS
  return true;
}

auto scheduled_actor::activate(scheduler* sched,
                               mailbox_element& x) -> activation_result {
  auto lg = log::core::trace("x = {}", x);
  if (!activate(sched))
    return activation_result::terminated;
  auto res = reactivate(x);
  if (res == activation_result::success)
    set_receive_timeout();
  return res;
}

auto scheduled_actor::reactivate(mailbox_element& x) -> activation_result {
  auto lg = log::core::trace("x = {}", x);
#ifdef CAF_ENABLE_EXCEPTIONS
  auto handle_exception = [&](std::exception_ptr eptr) {
    auto err = call_handler(exception_handler_, this, eptr);
    auto rp = make_response_promise();
    rp.deliver(err);
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
          log::core::debug("actor finalized");
          return activation_result::terminated;
        }
        return activation_result::success;
      case invoke_message_result::skipped:
        return activation_result::skipped;
    }
#ifdef CAF_ENABLE_EXCEPTIONS
  } catch (std::exception& e) {
    log::core::info("actor died because of an exception, what: {}", e.what());
    static_cast<void>(e); // keep compiler happy when not logging
    handle_exception(std::current_exception());
  } catch (...) {
    log::core::info("actor died because of an unknown exception");
    handle_exception(std::current_exception());
  }
  finalize();
#endif // CAF_ENABLE_EXCEPTIONS
  return activation_result::terminated;
}

// -- behavior management ----------------------------------------------------

void scheduled_actor::do_become(behavior bhvr, bool discard_old) {
  if (getf(is_terminated_flag | is_shutting_down_flag)) {
    log::system::warning("called become() on a terminated actor");
    return;
  }
  if (discard_old && !bhvr_stack_.empty())
    bhvr_stack_.pop_back();
  auto has_timeout = false;
  if (bhvr) {
    has_timeout = bhvr.timeout() != infinite;
    bhvr_stack_.push_back(std::move(bhvr));
  }
  if (has_timeout) {
    timeout_state_.mode = timeout_mode::legacy;
    timeout_state_.delay = bhvr_stack_.back().timeout();
    set_receive_timeout();
    return;
  }
  if (timeout_state_.mode == timeout_mode::legacy) {
    timeout_state_.pending.dispose();
    timeout_state_.reset();
  }
}

bool scheduled_actor::finalize() {
  auto lg = log::core::trace("");
  // Repeated calls always return `true` but have no side effects.
  if (is_terminated())
    return true;
  // An actor is considered alive as long as it has a behavior, didn't set
  // the terminated flag and has no watched flows remaining.
  run_actions();
  if (alive())
    return false;
  log::core::debug("actor has no behavior and is ready for cleanup");
  CAF_ASSERT(!has_behavior());
  bhvr_stack_.cleanup();
  cleanup(std::move(fail_state_), context());
  CAF_ASSERT(is_terminated());
  return true;
}

void scheduled_actor::push_to_cache(mailbox_element_ptr ptr) {
  stash_.push(ptr.release());
}

void scheduled_actor::call_error_handler(error& err) {
  call_handler(error_handler_, this, err);
}

disposable scheduled_actor::run_scheduled(timestamp when, action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  auto lg = log::core::trace("when = {}", when);
  auto delay = when - make_timestamp();
  return run_scheduled(clock().now() + delay, std::move(what));
}

disposable scheduled_actor::run_scheduled(actor_clock::time_point when,
                                          action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  auto lg = log::core::trace("when = {}",
                             const_cast<const actor_clock::time_point*>(&when));
  return clock().schedule(when, std::move(what), strong_actor_ptr{ctrl()});
}

disposable scheduled_actor::run_scheduled_weak(timestamp when, action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  auto lg = log::core::trace("when = {}", when);
  auto delay = when - make_timestamp();
  return run_scheduled_weak(clock().now() + delay, std::move(what));
}

disposable scheduled_actor::run_scheduled_weak(actor_clock::time_point when,
                                               action what) {
  CAF_ASSERT(what.ptr() != nullptr);
  auto lg = log::core::trace("when = {}",
                             const_cast<const actor_clock::time_point*>(&when));
  return clock().schedule(when, std::move(what), weak_actor_ptr{ctrl()});
}

disposable scheduled_actor::run_delayed(timespan delay, action what) {
  auto lg = log::core::trace("delay = {}", delay);
  return run_scheduled(clock().now() + delay, std::move(what));
}

disposable scheduled_actor::run_delayed_weak(timespan delay, action what) {
  auto lg = log::core::trace("delay = {}", delay);
  return run_scheduled_weak(clock().now() + delay, std::move(what));
}

// -- caf::flow bindings -------------------------------------------------------

flow::coordinator* scheduled_actor::flow_context() {
  return this;
}

stream scheduled_actor::to_stream_impl(cow_string name, batch_op_ptr batch_op,
                                       type_id_t item_type,
                                       size_t max_items_per_batch) {
  auto lg = log::core::trace("name = {}, item_type = {}", name,
                             query_type_name(item_type));
  auto local_id = new_u64_id();
  stream_sources_.emplace(local_id, stream_source_state{std::move(batch_op),
                                                        max_items_per_batch});
  return {ctrl(), item_type, std::move(name), local_id};
}

flow::observable<async::batch>
scheduled_actor::do_observe(stream what, size_t buf_capacity,
                            size_t request_threshold) {
  auto lg
    = log::core::trace("what = {}, buf_capacity = {}, request_threshold = {}",
                       what, buf_capacity, request_threshold);
  if (const auto& src = what.source()) {
    auto ptr = make_counted<detail::stream_bridge>(this, src, what.id(),
                                                   buf_capacity,
                                                   request_threshold);
    return flow::observable<async::batch>{std::move(ptr)};
  }
  return make_observable().fail<async::batch>(make_error(sec::invalid_stream));
}

void scheduled_actor::release_later(flow::coordinated_ptr& child) {
  CAF_ASSERT(child != nullptr);
  released_.emplace_back().swap(child);
}

void scheduled_actor::watch(disposable obj) {
  CAF_ASSERT(obj.valid());
  watched_disposables_.emplace_back(std::move(obj));
  log::core::debug("now watching {} disposables", watched_disposables_.size());
}

void scheduled_actor::deregister_stream(uint64_t stream_id) {
  stream_sources_.erase(stream_id);
}

void scheduled_actor::run_actions() {
  auto lg = log::core::trace("");
  delayed_actions_this_run_ = 0;
  if (!actions_.empty()) {
    // Note: if the first actions is null, it means that we are already running
    // actions right now. This can happen if an action calls `quit`, which will
    // call `run_actions` again.
    if (!actions_.front())
      return;
    // Note: can't use iterators here since actions may add to the vector.
    for (auto index = size_t{0}; index < actions_.size(); ++index) {
      action f;
      f.swap(actions_[index]);
      f.run();
    }
    actions_.clear();
  }
  released_.clear();
  update_watched_disposables();
}

void scheduled_actor::update_watched_disposables() {
  auto lg = log::core::trace("");
  [[maybe_unused]] auto n = disposable::erase_disposed(watched_disposables_);
  if (n > 0) {
    log::core::debug("now watching {} disposables",
                     watched_disposables_.size());
  }
}

void scheduled_actor::register_flow_state(uint64_t local_id,
                                          detail::stream_bridge_sub_ptr sub) {
  stream_bridges_.emplace(local_id, std::move(sub));
}

void scheduled_actor::drop_flow_state(uint64_t local_id) {
  stream_bridges_.erase(local_id);
}

void scheduled_actor::try_push_stream(uint64_t local_id) {
  auto lg = log::core::trace("local_id = {}", local_id);
  if (auto i = stream_bridges_.find(local_id); i != stream_bridges_.end())
    i->second->push();
}

void scheduled_actor::unstash() {
  while (auto stashed = stash_.pop())
    mailbox().push_front(mailbox_element_ptr{stashed});
}

void scheduled_actor::do_unstash(mailbox_element_ptr ptr) {
  mailbox().push_front(std::move(ptr));
}

void scheduled_actor::cancel_flows_and_streams() {
  // Note: we always swap out a map before iterating it, because some callbacks
  //       may call erase on the map while we are iterating it.
  stream_sources_.clear();
  if (!stream_subs_.empty()) {
    decltype(stream_subs_) subs;
    subs.swap(stream_subs_);
    for (auto& [id, ptr] : subs)
      ptr->cancel();
  }
  if (!stream_bridges_.empty()) {
    decltype(stream_bridges_) bridges;
    bridges.swap(stream_bridges_);
    for (auto& [id, ptr] : bridges)
      ptr->drop();
  }
  while (!watched_disposables_.empty()) {
    decltype(watched_disposables_) disposables;
    disposables.swap(watched_disposables_);
    for (auto& ptr : disposables)
      ptr.dispose();
  }
  run_actions();
}

void scheduled_actor::close_mailbox(const error& reason) {
  // Discard stashed messages.
  auto dropped = size_t{0};
  if (!stash_.empty()) {
    detail::sync_request_bouncer bounce{reason};
    while (auto stashed = stash_.pop()) {
      bounce(*stashed);
      delete stashed;
      ++dropped;
    }
  }
  // Clear mailbox.
  if (!mailbox().closed())
    dropped += mailbox().close(reason);
  if (dropped > 0 && metrics_.mailbox_size)
    metrics_.mailbox_size->dec(static_cast<int64_t>(dropped));
}

void scheduled_actor::force_close_mailbox() {
  close_mailbox(make_error(exit_reason::unreachable));
}

// -- monitoring ---------------------------------------------------------------

disposable
scheduled_actor::do_monitor(abstract_actor* ptr,
                            detail::abstract_monitor_action_ptr on_down) {
  if (ptr == nullptr)
    return {};
  ptr->attach_functor([self = address(), on_down](error reason) {
    // Failing to set the arg means the action was disposed.
    if (on_down->set_reason(std::move(reason))) {
      if (auto shdl = actor_cast<actor>(self))
        shdl->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           action{on_down}),
                      nullptr);
    }
  });
  return on_down->as_disposable();
}

} // namespace caf
