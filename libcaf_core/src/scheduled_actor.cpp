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

#include "caf/scheduled_actor.hpp"

#include "caf/detail/private_thread.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {

scheduled_actor::scheduled_actor(actor_config& cfg) : local_actor(cfg) {
  // nop
}

scheduled_actor::~scheduled_actor() {
  // nop
}

void scheduled_actor::enqueue(mailbox_element_ptr ptr,
                                         execution_unit* eu) {
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(! is_blocking());
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  switch (mailbox().enqueue(ptr.release())) {
    case detail::enqueue_result::unblocked_reader: {
      // add a reference count to this actor and re-schedule it
      intrusive_ptr_add_ref(ctrl());
      if (is_detached()) {
        CAF_ASSERT(private_thread_ != nullptr);
        private_thread_->resume();
      } else {
        if (eu)
          eu->exec_later(this);
        else
          home_system().scheduler().enqueue(this);
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

void scheduled_actor::launch(execution_unit* eu, bool lazy, bool hide) {
  CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
  CAF_ASSERT(! is_blocking());
  is_registered(! hide);
  if (is_detached()) {
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

resumable::subtype_t scheduled_actor::subtype() const {
  return resumable::scheduled_actor;
}

void scheduled_actor::intrusive_ptr_add_ref_impl() {
  intrusive_ptr_add_ref(ctrl());
}

void scheduled_actor::intrusive_ptr_release_impl() {
  intrusive_ptr_release(ctrl());
}

resumable::resume_result
scheduled_actor::resume(execution_unit* eu, size_t max_throughput) {
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE("");
  CAF_ASSERT(eu != nullptr);
  CAF_ASSERT(! is_blocking());
  context(eu);
  if (is_initialized() && (! has_behavior() || is_terminated())) {
    CAF_LOG_DEBUG_IF(! has_behavior(),
                     "resume called on an actor without behavior");
    CAF_LOG_DEBUG_IF(is_terminated(),
                     "resume called on a terminated actor");
    return resumable::done;
  }
  std::exception_ptr eptr = nullptr;
  try {
    if (! is_initialized()) {
      initialize();
      if (finished()) {
        CAF_LOG_DEBUG("actor_done() returned true right "
                      << "after make_behavior()");
        return resumable::resume_result::done;
      } else {
        CAF_LOG_DEBUG("initialized actor:" << CAF_ARG(name()));
      }
    }
    int handled_msgs = 0;
    auto reset_timeout_if_needed = [&] {
      if (handled_msgs > 0 && ! bhvr_stack_.empty()) {
        request_timeout(bhvr_stack_.back().timeout());
      }
    };
    for (size_t i = 0; i < max_throughput; ++i) {
      auto ptr = next_message();
      if (ptr) {
        auto res = exec_event(ptr);
        if (res.first == resumable::resume_result::done)
          return resumable::resume_result::done;
        if (res.second == im_success)
          ++handled_msgs;
      } else {
        CAF_LOG_DEBUG("no more element in mailbox; going to block");
        reset_timeout_if_needed();
        if (mailbox().try_block())
          return resumable::awaiting_message;
        CAF_LOG_DEBUG("try_block() interrupted by new message");
      }
    }
    reset_timeout_if_needed();
    if (! has_next_message() && mailbox().try_block())
      return resumable::awaiting_message;
    // time's up
    return resumable::resume_later;
  }
  catch (std::exception& e) {
    CAF_LOG_INFO("actor died because of an exception, what: " << e.what());
    static_cast<void>(e); // keep compiler happy when not logging
    if (! is_terminated())
      quit(exit_reason::unhandled_exception);
    eptr = std::current_exception();
  }
  catch (...) {
    CAF_LOG_INFO("actor died because of an unknown exception");
    if (! is_terminated())
      quit(exit_reason::unhandled_exception);
    eptr = std::current_exception();
  }
  if (eptr) {
    auto opt_reason = handle(eptr);
    if (opt_reason) {
      // use exit reason defined by custom handler
      quit(*opt_reason);
    }
  }
  if (! finished()) {
    // actor has been "revived", try running it again later
    return resumable::resume_later;
  }
  return resumable::done;
}

void scheduled_actor::quit(error x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  fail_state_ = std::move(x);
  is_terminated(true);
}

void scheduled_actor::exec_single_event(execution_unit* ctx,
                                        mailbox_element_ptr& ptr) {
  CAF_ASSERT(ctx != nullptr);
  context(ctx);
  if (! is_initialized()) {
    CAF_LOG_DEBUG("initialize actor");
    initialize();
    if (finished()) {
      CAF_LOG_DEBUG("actor_done() returned true right "
                    << "after make_behavior()");
      return;
    }
  }
  if (! has_behavior() || is_terminated()) {
    CAF_LOG_DEBUG_IF(! has_behavior(),
                     "resume called on an actor without behavior");
    CAF_LOG_DEBUG_IF(is_terminated(),
                     "resume called on a terminated actor");
    return;
  }
  try {
    exec_event(ptr);
  }
  catch (...) {
    CAF_LOG_INFO("broker died because of an exception");
    auto eptr = std::current_exception();
    auto opt_reason = this->handle(eptr);
    if (opt_reason)
      quit(*opt_reason);
  }
}

} // namespace caf
