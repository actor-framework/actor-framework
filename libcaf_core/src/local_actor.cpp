/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <string>

#include "caf/all.hpp"
#include "caf/atom.hpp"
#include "caf/actor_cast.hpp"
#include "caf/local_actor.hpp"
#include "caf/default_attachable.hpp"

#include "caf/scheduler/detached_threads.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {

// local actors are created with a reference count of one that is adjusted
// later on in spawn(); this prevents subtle bugs that lead to segfaults,
// e.g., when calling address() in the ctor of a derived class
local_actor::local_actor()
    : planned_exit_reason_(exit_reason::not_exited),
      timeout_id_(0) {
  // nop
}

local_actor::~local_actor() {
  if (! mailbox_.closed()) {
    detail::sync_request_bouncer f{this->exit_reason()};
    mailbox_.close(f);
  }
}

void local_actor::monitor(const actor_addr& whom) {
  if (whom == invalid_actor_addr) {
    return;
  }
  auto ptr = actor_cast<abstract_actor_ptr>(whom);
  ptr->attach(default_attachable::make_monitor(address()));
}

void local_actor::demonitor(const actor_addr& whom) {
  if (whom == invalid_actor_addr) {
    return;
  }
  auto ptr = actor_cast<abstract_actor_ptr>(whom);
  default_attachable::observe_token tk{address(), default_attachable::monitor};
  ptr->detach(tk);
}

void local_actor::join(const group& what) {
  CAF_LOG_TRACE(CAF_TSARG(what));
  if (what == invalid_group) {
    return;
  }
  abstract_group::subscription_token tk{what.ptr()};
  std::unique_lock<std::mutex> guard{mtx_};
  if (detach_impl(tk, attachables_head_, true, true) == 0) {
    auto ptr = what->subscribe(address());
    if (ptr) {
      attach_impl(ptr);
    }
  }
}

void local_actor::leave(const group& what) {
  CAF_LOG_TRACE(CAF_TSARG(what));
  if (what == invalid_group) {
    return;
  }
  if (detach(abstract_group::subscription_token{what.ptr()}) > 0) {
    what->unsubscribe(address());
  }
}

void local_actor::on_exit() {
  // nop
}

std::vector<group> local_actor::joined_groups() const {
  std::vector<group> result;
  result.reserve(20);
  attachable::token stk{attachable::token::subscription, nullptr};
  std::unique_lock<std::mutex> guard{mtx_};
  for (attachable* i = attachables_head_.get(); i != 0; i = i->next.get()) {
    if (i->matches(stk)) {
      auto ptr = static_cast<abstract_group::subscription*>(i);
      result.emplace_back(ptr->group());
    }
  }
  return result;
}

void local_actor::forward_message(const actor& dest, message_priority prio) {
  if (! dest) {
    return;
  }
  auto mid = current_element_->mid;
  current_element_->mid = prio == message_priority::high
                           ? mid.with_high_priority()
                           : mid.with_normal_priority();
  dest->enqueue(std::move(current_element_), host());
}

uint32_t local_actor::request_timeout(const duration& d) {
  if (! d.valid()) {
    has_timeout(false);
    return 0;
  }
  has_timeout(true);
  auto result = ++timeout_id_;
  auto msg = make_message(timeout_msg{++timeout_id_});
  if (d.is_zero()) {
    // immediately enqueue timeout message if duration == 0s
    enqueue(address(), invalid_message_id, std::move(msg), host());
  } else {
    delayed_send(this, d, std::move(msg));
  }
  return result;
}

void local_actor::request_sync_timeout_msg(const duration& d, message_id mid) {
  if (! d.valid()) {
    return;
  }
  delayed_send_impl(mid, this, d, make_message(sync_timeout_msg{}));
}

void local_actor::handle_timeout(behavior& bhvr, uint32_t timeout_id) {
  if (! is_active_timeout(timeout_id)) {
    return;
  }
  bhvr.handle_timeout();
  if (bhvr_stack_.empty() || bhvr_stack_.back() != bhvr) {
    return;
  }
  // auto-remove behavior for blocking actors
  if (is_blocking()) {
    CAF_ASSERT(bhvr_stack_.back() == bhvr);
    bhvr_stack_.pop_back();
    return;
  }
}

void local_actor::reset_timeout(uint32_t timeout_id) {
  if (is_active_timeout(timeout_id)) {
    has_timeout(false);
  }
}

bool local_actor::is_active_timeout(uint32_t tid) const {
  return has_timeout() && timeout_id_ == tid;
}

uint32_t local_actor::active_timeout_id() const {
  return timeout_id_;
}

enum class msg_type {
  normal_exit,           // an exit message with normal exit reason
  non_normal_exit,       // an exit message with abnormal exit reason
  expired_timeout,       // an 'old & obsolete' timeout
  expired_sync_response, // a sync response that already timed out
  timeout,               // triggers currently active timeout
  ordinary,              // an asynchronous message or sync. request
  sync_response          // a synchronous response
};

msg_type filter_msg(local_actor* self, mailbox_element& node) {
  const message& msg = node.msg;
  auto mid = node.mid;
  if (mid.is_response()) {
    return self->awaits(mid) ? msg_type::sync_response
                             : msg_type::expired_sync_response;
  }
  if (msg.size() != 1) {
    return msg_type::ordinary;
  }
  if (msg.match_element<timeout_msg>(0)) {
    auto& tm = msg.get_as<timeout_msg>(0);
    auto tid = tm.timeout_id;
    CAF_ASSERT(! mid.valid());
    if (self->is_active_timeout(tid)) {
      return msg_type::timeout;
    }
    return msg_type::expired_timeout;
  }
  if (msg.match_element<exit_msg>(0)) {
    auto& em = msg.get_as<exit_msg>(0);
    CAF_ASSERT(! mid.valid());
    // make sure to get rid of attachables if they're no longer needed
    self->unlink_from(em.source);
    if (em.reason == exit_reason::kill) {
      self->quit(em.reason);
      return msg_type::non_normal_exit;
    }
    if (self->trap_exit() == false) {
      if (em.reason != exit_reason::normal) {
        self->quit(em.reason);
        return msg_type::non_normal_exit;
      }
      return msg_type::normal_exit;
    }
  }
  return msg_type::ordinary;
}

response_promise fetch_response_promise(local_actor* self, int) {
  return self->make_response_promise();
}

response_promise fetch_response_promise(local_actor*, response_promise& hdl) {
  return std::move(hdl);
}

// enables `return sync_send(...).then(...)`
bool handle_message_id_res(local_actor* self, message& res,
                           optional<response_promise> hdl) {
  if (res.match_elements<atom_value, uint64_t>()
      && res.get_as<atom_value>(0) == atom("MESSAGE_ID")) {
    CAF_LOGF_DEBUG("message handler returned a message id wrapper");
    auto id = res.get_as<uint64_t>(1);
    auto msg_id = message_id::from_integer_value(id);
    auto ref_opt = self->find_pending_response(msg_id);
    // install a behavior that calls the user-defined behavior
    // and using the result of its inner behavior as response
    if (ref_opt) {
      response_promise fhdl = hdl ? *hdl : self->make_response_promise();
      behavior& ref = std::get<1>(*ref_opt);
      behavior inner = ref;
      ref.assign(
        others >> [=] {
          // inner is const inside this lambda and mutable a C++14 feature
          behavior cpy = inner;
          auto inner_res = cpy(self->current_message());
          if (inner_res && ! handle_message_id_res(self, *inner_res, fhdl)) {
            fhdl.deliver(*inner_res);
          }
        }
      );
      return true;
    }
  }
  return false;
}

// - extracts response message from handler
// - returns true if fun was successfully invoked
template <class Handle = int>
optional<message> post_process_invoke_res(local_actor* self,
                                          message_id mid,
                                          optional<message>&& res,
                                          Handle hdl = Handle{}) {
  CAF_LOGF_TRACE(CAF_MARG(mid, integer_value) << ", " << CAF_TSARG(res));
  if (! res) {
    return none;
  }
  if (res->empty()) {
    // make sure synchronous requests always receive a response;
    // note: ! current_mailbox_element() means client has forwarded the request
    auto& ptr = self->current_mailbox_element();
    if (ptr) {
      mid = ptr->mid;
      if (mid.is_request() && ! mid.is_answered()) {
        auto fhdl = fetch_response_promise(self, hdl);
        if (fhdl) {
          fhdl.deliver(message{});
        }
      }
    }
    return res;
  }
  if (handle_message_id_res(self, *res, none)) {
    return message{};
  }
  // respond by using the result of 'fun'
  CAF_LOGF_DEBUG("respond via response_promise");
  auto fhdl = fetch_response_promise(self, hdl);
  if (fhdl) {
    fhdl.deliver(std::move(*res));
    // inform caller about success by returning not none
    return message{};
  }
  return res;
}

invoke_message_result local_actor::invoke_message(mailbox_element_ptr& ptr,
                                                  behavior& fun,
                                                  message_id awaited_id) {
  CAF_ASSERT(ptr != nullptr);
  CAF_LOG_TRACE(CAF_TSARG(*ptr) << ", " << CAF_MARG(awaited_id, integer_value));
  switch (filter_msg(this, *ptr)) {
    case msg_type::normal_exit:
      CAF_LOG_DEBUG("dropped normal exit signal");
      return im_dropped;
    case msg_type::expired_sync_response:
      CAF_LOG_DEBUG("dropped expired sync response");
      return im_dropped;
    case msg_type::expired_timeout:
      CAF_LOG_DEBUG("dropped expired timeout message");
      return im_dropped;
    case msg_type::non_normal_exit:
      CAF_LOG_DEBUG("handled non-normal exit signal");
      // this message was handled
      // by calling quit(...)
      return im_success;
    case msg_type::timeout: {
      if (awaited_id == invalid_message_id) {
        CAF_LOG_DEBUG("handle timeout message");
        auto& tm = ptr->msg.get_as<timeout_msg>(0);
        handle_timeout(fun, tm.timeout_id);
        return im_success;
      }
      // ignore "async" timeout
      CAF_LOG_DEBUG("async timeout ignored while in sync mode");
      return im_dropped;
    }
    case msg_type::sync_response:
      CAF_LOG_DEBUG("handle as synchronous response: "
                    << CAF_TARG(ptr->msg, to_string) << ", "
                    << CAF_MARG(ptr->mid, integer_value) << ", "
                    << CAF_MARG(awaited_id, integer_value));
      if (awaited_id.valid() && ptr->mid == awaited_id) {
        bool is_sync_tout = ptr->msg.match_elements<sync_timeout_msg>();
        ptr.swap(current_mailbox_element());
        auto mid = current_mailbox_element()->mid;
        if (is_sync_tout) {
          if (fun.timeout().valid()) {
            fun.handle_timeout();
          }
        } else {
          auto res = post_process_invoke_res(this, mid,
                                             fun(current_mailbox_element()->msg));
          ptr.swap(current_mailbox_element());
          if (! res) {
            CAF_LOG_WARNING("sync failure occured in actor "
                              << "with ID " << id());
            handle_sync_failure();
          }
        }
        ptr.swap(current_mailbox_element());
        mark_arrived(awaited_id);
        return im_success;
      }
      return im_skipped;
    case msg_type::ordinary:
      if (! awaited_id.valid()) {
        auto had_timeout = has_timeout();
        if (had_timeout) {
          has_timeout(false);
        }
        ptr.swap(current_mailbox_element());
        auto mid = current_mailbox_element()->mid;
        auto res = post_process_invoke_res(this, mid,
                                           fun(current_mailbox_element()->msg));
        ptr.swap(current_mailbox_element());
        if (res) {
          return im_success;
        }
        // restore timeout if necessary
        if (had_timeout) {
          has_timeout(true);
        }
      }
      CAF_LOG_DEBUG_IF(awaited_id.valid(),
                "ignored message; await response: "
                  << awaited_id.integer_value());
      return im_skipped;
  }
  // should be unreachable
  CAF_CRITICAL("invalid message type");
}

struct pending_response_predicate {
public:
  explicit pending_response_predicate(message_id mid) : mid_(mid) {
    // nop
  }

  bool operator()(const local_actor::pending_response& pr) const {
    return std::get<0>(pr) == mid_;
  }

private:
  message_id mid_;
};

message_id local_actor::new_request_id(message_priority mp) {
  auto result = ++last_request_id_;
  pending_responses_.emplace_front(result.response_id(), behavior{});
  return mp == message_priority::normal ? result : result.with_high_priority();
}

void local_actor::mark_arrived(message_id mid) {
  CAF_ASSERT(mid.is_response());
  pending_response_predicate predicate{mid};
  pending_responses_.remove_if(predicate);
}

bool local_actor::awaits_response() const {
  return ! pending_responses_.empty();
}

bool local_actor::awaits(message_id mid) const {
  CAF_ASSERT(mid.is_response());
  pending_response_predicate predicate{mid};
  return std::any_of(pending_responses_.begin(), pending_responses_.end(),
                     predicate);
}

optional<local_actor::pending_response&>
local_actor::find_pending_response(message_id mid) {
  pending_response_predicate predicate{mid};
  auto last = pending_responses_.end();
  auto i = std::find_if(pending_responses_.begin(), last, predicate);
  if (i == last) {
    return none;
  }
  return *i;
}

void local_actor::set_response_handler(message_id response_id, behavior bhvr) {
  auto pr = find_pending_response(response_id);
  if (pr) {
    if (bhvr.timeout().valid()) {
      request_sync_timeout_msg(bhvr.timeout(), response_id);
    }
    pr->second = std::move(bhvr);
  }
}

behavior& local_actor::awaited_response_handler() {
  return pending_responses_.front().second;
}

message_id local_actor::awaited_response_id() {
  return pending_responses_.empty()
         ? message_id::make()
         : pending_responses_.front().first;
}

void local_actor::launch(execution_unit* eu, bool lazy, bool hide) {
  is_registered(! hide);
  if (is_detached()) {
    // actor lives in its own thread
    CAF_PUSH_AID(id());
    CAF_LOG_TRACE(CAF_ARG(lazy) << ", " << CAF_ARG(hide));
    scheduler::inc_detached_threads();
    //intrusive_ptr<local_actor> mself{this};
    std::thread{[hide](intrusive_ptr<local_actor> mself) {
      // this extra scope makes sure that the trace logger is
      // destructed before dec_detached_threads() is called
      {
        CAF_PUSH_AID(mself->id());
        CAF_LOGF_TRACE("");
        auto max_throughput = std::numeric_limits<size_t>::max();
        while (mself->resume(nullptr, max_throughput) != resumable::done) {
          // await new data before resuming actor
          mself->await_data();
          CAF_ASSERT(mself->mailbox().blocked() == false);
        }
        mself.reset();
      }
      scheduler::dec_detached_threads();
    }, intrusive_ptr<local_actor>{this}}.detach();
    return;
  }
  // actor is cooperatively scheduled
  attach_to_scheduler();
  // do not schedule immediately when spawned with `lazy_init`
  // mailbox could be set to blocked
  if (lazy && mailbox().try_block()) {
    return;
  }
  if (eu) {
    eu->exec_later(this);
  } else {
    detail::singletons::get_scheduling_coordinator()->enqueue(this);
  }
}

void local_actor::enqueue(const actor_addr& sender, message_id mid,
                          message msg, execution_unit* eu) {
  enqueue(mailbox_element::make(sender, mid, std::move(msg)), eu);
}

void local_actor::enqueue(mailbox_element_ptr ptr, execution_unit* eu) {
  if (is_detached()) {
    // actor lives in its own thread
    auto mid = ptr->mid;
    auto sender = ptr->sender;
    // returns false if mailbox has been closed
    if (! mailbox().synchronized_enqueue(mtx_, cv_, ptr.release())) {
      if (mid.is_request()) {
        detail::sync_request_bouncer srb{exit_reason()};
        srb(sender, mid);
      }
    }
    return;
  }
  // actor is cooperatively scheduled
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  switch (mailbox().enqueue(ptr.release())) {
    case detail::enqueue_result::unblocked_reader: {
      // re-schedule actor
      if (eu) {
        eu->exec_later(this);
      } else {
        detail::singletons::get_scheduling_coordinator()->enqueue(this);
      }
      break;
    }
    case detail::enqueue_result::queue_closed: {
      if (mid.is_request()) {
        detail::sync_request_bouncer f{exit_reason()};
        f(sender, mid);
      }
      break;
    }
    case detail::enqueue_result::success:
      // enqueued to a running actors' mailbox; nothing to do
      break;
  }
}

void local_actor::attach_to_scheduler() {
  ref();
}

void local_actor::detach_from_scheduler() {
  deref();
}

resumable::resume_result local_actor::resume(execution_unit* eu,
                                             size_t max_throughput) {
  CAF_LOG_TRACE("");
  if (is_blocking()) {
    // actor lives in its own thread
    CAF_ASSERT(dynamic_cast<blocking_actor*>(this) != 0);
    auto self = static_cast<blocking_actor*>(this);
    uint32_t rsn = exit_reason::normal;
    std::exception_ptr eptr = nullptr;
    try {
      self->act();
    }
    catch (actor_exited& e) {
      rsn = e.reason();
    }
    catch (...) {
      rsn = exit_reason::unhandled_exception;
      eptr = std::current_exception();
    }
    if (eptr) {
      auto opt_reason = self->handle(eptr);
      rsn = *opt_reason;
    }
    self->planned_exit_reason(rsn);
    try {
      self->on_exit();
    }
    catch (...) {
      // simply ignore exception
    }
    // exit reason might have been changed by on_exit()
    self->cleanup(self->planned_exit_reason());
    return resumable::done;
  }
  // actor is cooperatively scheduled
  host(eu);
  auto actor_done = [&]() -> bool {
    if (! has_behavior() || planned_exit_reason() != exit_reason::not_exited) {
      CAF_LOG_DEBUG("actor either has no behavior or has set an exit reason");
      on_exit();
      bhvr_stack().clear();
      bhvr_stack().cleanup();
      auto rsn = planned_exit_reason();
      if (rsn == exit_reason::not_exited) {
        rsn = exit_reason::normal;
        planned_exit_reason(rsn);
      }
      cleanup(rsn);
      return true;
    }
    return false;
  };
  // actors without behavior or that have already defined
  // an exit reason must not be resumed
  CAF_ASSERT(! is_initialized()
             || (has_behavior()
                 && planned_exit_reason() == exit_reason::not_exited));
  std::exception_ptr eptr = nullptr;
  try {
    if (! is_initialized()) {
      CAF_LOG_DEBUG("initialize actor");
      initialize();
      if (actor_done()) {
        CAF_LOG_DEBUG("actor_done() returned true right "
                      << "after make_behavior()");
        return resumable::resume_result::done;
      }
    }
    int handled_msgs = 0;
    auto reset_timeout_if_needed = [&] {
      if (handled_msgs > 0) {
        request_timeout(get_behavior().timeout());
      }
    };
    for (size_t i = 0; i < max_throughput; ++i) {
      auto ptr = next_message();
      if (ptr) {
        auto& bhvr = awaits_response()
                     ? awaited_response_handler()
                     : bhvr_stack().back();
        auto mid = awaited_response_id();
        switch (invoke_message(ptr, bhvr, mid)) {
          case im_success:
            bhvr_stack().cleanup();
            ++handled_msgs;
            if (actor_done()) {
              CAF_LOG_DEBUG("actor exited");
              return resumable::resume_result::done;
            }
            // continue from cache if current message was
            // handled, because the actor might have changed
            // its behavior to match 'old' messages now
            while (invoke_from_cache()) {
              if (actor_done()) {
                CAF_LOG_DEBUG("actor exited");
                return resumable::resume_result::done;
              }
            }
            break;
          case im_skipped:
            CAF_ASSERT(ptr != nullptr);
            push_to_cache(std::move(ptr));
            break;
          case im_dropped:
            // destroy msg
            break;
        }
      } else {
        CAF_LOG_DEBUG("no more element in mailbox; going to block");
        reset_timeout_if_needed();
        if (mailbox().try_block()) {
          return resumable::awaiting_message;
        }
        CAF_LOG_DEBUG("try_block() interrupted by new message");
      }
    }
    reset_timeout_if_needed();
    if (! has_next_message() && mailbox().try_block())
      return resumable::awaiting_message;
    // time's up
    return resumable::resume_later;
  }
  catch (actor_exited& what) {
    CAF_LOG_INFO("actor died because of exception: actor_exited, "
                 "reason = " << what.reason());
    if (exit_reason() == exit_reason::not_exited) {
      quit(what.reason());
    }
  }
  catch (std::exception& e) {
    CAF_LOG_INFO("actor died because of an exception, what: " << e.what());
    static_cast<void>(e); // keep compiler happy when not logging
    if (exit_reason() == exit_reason::not_exited) {
      quit(exit_reason::unhandled_exception);
    }
    eptr = std::current_exception();
  }
  catch (...) {
    CAF_LOG_INFO("actor died because of an unknown exception");
    if (exit_reason() == exit_reason::not_exited) {
      quit(exit_reason::unhandled_exception);
    }
    eptr = std::current_exception();
  }
  if (eptr) {
    auto opt_reason = handle(eptr);
    if (opt_reason) {
      // use exit reason defined by custom handler
      planned_exit_reason(*opt_reason);
    }
  }
  if (! actor_done()) {
    // actor has been "revived", try running it again later
    return resumable::resume_later;
  }
  return resumable::done;
}

mailbox_element_ptr local_actor::next_message() {
  if (! is_priority_aware()) {
    return mailbox_element_ptr{mailbox().try_pop()};
  }
  // we partition the mailbox into four segments in this case:
  // <-------- ! was_skipped --------> | <--------  was_skipped -------->
  // <-- high prio --><-- low prio -->|<-- high prio --><-- low prio -->
  auto& cache = mailbox().cache();
  auto i = cache.first_begin();
  auto e = cache.first_end();
  if (i == e || ! i->is_high_priority()) {
    // insert points for high priority
    auto hp_pos = i;
    // read whole mailbox at once
    auto tmp = mailbox().try_pop();
    while (tmp) {
      cache.insert(tmp->is_high_priority() ? hp_pos : e, tmp);
      // adjust high priority insert point on first low prio element insert
      if (hp_pos == e && ! tmp->is_high_priority()) {
        --hp_pos;
      }
      tmp = mailbox().try_pop();
    }
  }
  mailbox_element_ptr result;
  if (! cache.first_empty()) {
    result.reset(cache.take_first_front());
  }
  return result;
}

bool local_actor::has_next_message() {
  if (! is_priority_aware()) {
    return mailbox_.can_fetch_more();
  }
  auto& mbox = mailbox();
  auto& cache = mbox.cache();
  return ! cache.first_empty() || mbox.can_fetch_more();
}

void local_actor::push_to_cache(mailbox_element_ptr ptr) {
  if (! is_priority_aware()) {
    mailbox().cache().push_second_back(ptr.release());
    return;
  }
  auto high_prio = [](const mailbox_element& val) {
    return val.is_high_priority();
  };
  auto& cache = mailbox().cache();
  auto e = cache.second_end();
  auto i = ptr->is_high_priority()
           ? std::partition_point(cache.second_begin(), e, high_prio)
           : e;
  cache.insert(i, ptr.release());
}

bool local_actor::invoke_from_cache() {
  return invoke_from_cache(get_behavior(), awaited_response_id());
}

bool local_actor::invoke_from_cache(behavior& bhvr, message_id mid) {
  auto& cache = mailbox().cache();
  auto i = cache.second_begin();
  auto e = cache.second_end();
  CAF_LOG_DEBUG(std::distance(i, e) << " elements in cache");
  return cache.invoke(this, i, e, bhvr, mid);
}

void local_actor::do_become(behavior bhvr, bool discard_old) {
  if (discard_old) {
    bhvr_stack_.pop_back();
  }
  // request_timeout simply resets the timeout when it's invalid
  request_timeout(bhvr.timeout());
  bhvr_stack_.push_back(std::move(bhvr));
}

void local_actor::await_data() {
  if (has_next_message()) {
    return;
  }
  mailbox().synchronized_await(mtx_, cv_);
}

void local_actor::send_impl(message_id mid, abstract_channel* dest,
                            message what) const {
  if (! dest) {
    return;
  }
  dest->enqueue(address(), mid, std::move(what), host());
}

void local_actor::send_exit(const actor_addr& whom, uint32_t reason) {
  send(message_priority::high, actor_cast<actor>(whom),
       exit_msg{address(), reason});
}

void local_actor::delayed_send_impl(message_id mid, const channel& dest,
                                    const duration& rel_time, message msg) {
  auto sched_cd = detail::singletons::get_scheduling_coordinator();
  sched_cd->delayed_send(rel_time, address(), dest, mid, std::move(msg));
}

response_promise local_actor::make_response_promise() {
  auto& ptr = current_element_;
  if (! ptr) {
    return response_promise{};
  }
  response_promise result{address(), ptr->sender, ptr->mid.response_id()};
  ptr->mid.mark_as_answered();
  return result;
}

behavior& local_actor::get_behavior() {
  return pending_responses_.empty() ? bhvr_stack_.back()
                                    : pending_responses_.front().second;
}

void local_actor::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  current_mailbox_element().reset();
  detail::sync_request_bouncer f{reason};
  mailbox_.close(f);
  pending_responses_.clear();
  abstract_actor::cleanup(reason);
  // tell registry we're done
  is_registered(false);
}

void local_actor::quit(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  planned_exit_reason(reason);
  if (is_blocking()) {
    throw actor_exited(reason);
  }
}

// <backward_compatibility version="0.12">
message& local_actor::last_dequeued() {
  if (! current_element_) {
    auto errstr = "last_dequeued called after forward_to or not in a callback";
    CAF_LOG_ERROR(errstr);
    throw std::logic_error(errstr);
  }
  return current_element_->msg;
}

actor_addr& local_actor::last_sender() {
  if (! current_element_) {
    auto errstr = "last_sender called after forward_to or not in a callback";
    CAF_LOG_ERROR(errstr);
    throw std::logic_error(errstr);
  }
  return current_element_->sender;
}
// </backward_compatibility>

} // namespace caf
