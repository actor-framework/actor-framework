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

#include "caf/blocking_actor.hpp"

#include "caf/logger.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_registry.hpp"

#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"

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

blocking_actor::blocking_actor(actor_config& sys)
    : super(sys.add_flag(local_actor::is_blocking_flag)) {
  // nop
}

blocking_actor::~blocking_actor() {
  // avoid weak-vtables warning
}

void blocking_actor::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  auto mid = ptr->mid;
  auto src = ptr->sender;
  // returns false if mailbox has been closed
  if (! mailbox().synchronized_enqueue(mtx_, cv_, ptr.release())) {
    if (mid.is_request()) {
      detail::sync_request_bouncer srb{exit_reason()};
      srb(src, mid);
    }
  }
}

void blocking_actor::launch(execution_unit*, bool, bool hide) {
  CAF_LOG_TRACE(CAF_ARG(hide));
  CAF_ASSERT(is_blocking());
  is_registered(! hide);
  home_system().inc_detached_threads();
  std::thread([](strong_actor_ptr ptr) {
    // actor lives in its own thread
    auto this_ptr = ptr->get();
    CAF_ASSERT(dynamic_cast<blocking_actor*>(this_ptr) != 0);
    auto self = static_cast<blocking_actor*>(this_ptr);
    error rsn;
    std::exception_ptr eptr = nullptr;
    try {
      self->act();
      rsn = self->fail_state_;
    }
    catch (...) {
      rsn = exit_reason::unhandled_exception;
      eptr = std::current_exception();
    }
    if (eptr) {
      auto opt_reason = self->handle(eptr);
      rsn = opt_reason ? *opt_reason
                       : exit_reason::unhandled_exception;
    }
    try {
      self->on_exit();
    }
    catch (...) {
      // simply ignore exception
    }
    self->cleanup(std::move(rsn), self->context());
    ptr->home_system->dec_detached_threads();
  }, ctrl()).detach();
}

blocking_actor::receive_while_helper
blocking_actor::receive_while(std::function<bool()> stmt) {
  return {this, stmt};
}

blocking_actor::receive_while_helper
blocking_actor::receive_while(const bool& ref) {
  return receive_while([&] { return ref; });
}

void blocking_actor::await_all_other_actors_done() {
  system().registry().await_running_count_equal(is_registered() ? 1 : 0);
}

void blocking_actor::act() {
  CAF_LOG_TRACE("");
  if (initial_behavior_fac_)
    initial_behavior_fac_(this);
}

void blocking_actor::fail_state(error err) {
  fail_state_ = std::move(err);
}

void blocking_actor::receive_impl(receive_cond& rcc,
                                  message_id mid,
                                  detail::blocking_behavior& bhvr) {
  CAF_LOG_TRACE(CAF_ARG(mid));
  // calculate absolute timeout if requested by user
  auto rel_tout = bhvr.timeout();
  std::chrono::high_resolution_clock::time_point abs_tout;
  if (rel_tout.valid()) {
    abs_tout = std::chrono::high_resolution_clock::now();
    abs_tout += rel_tout;
  }
  // try to dequeue from cache first
  if (invoke_from_cache(bhvr.nested, mid))
    return;
  // read incoming messages until we have a match or a timeout
  detail::default_invoke_result_visitor visitor{this};
  for (;;) {
    if (! rcc.pre())
      return;
    bool skipped;
    do {
      skipped = false;
      if (rel_tout.valid()) {
        if (! await_data(abs_tout)) {
          bhvr.handle_timeout();
          return;
        }
      } else {
        await_data();
      }
      auto ptr = next_message();
      CAF_ASSERT(ptr != nullptr);
      // skip messages that don't match our message ID
      if (ptr->mid != mid) {
        push_to_cache(std::move(ptr));
        continue;
      }
      ptr.swap(current_element_);
      switch (bhvr.nested(visitor, current_element_->msg)) {
        case match_case::skip:
          skipped = true;
          break;
        default:
          break;
        case match_case::no_match: {
          auto sres = bhvr.fallback(current_element_->msg.cvals().get());
          // when dealing with response messages, there's either a match
          // on the first handler or we produce an error to
          // get a match on the second (error) handler
          if (sres.flag != rt_skip) {
            visitor.visit(sres);
          } else if (mid.valid()) {
            // make new message to replace current_element_->msg
            auto x = make_message(make_error(sec::unexpected_response,
                                             std::move(current_element_->msg)));
            current_element_->msg = std::move(x);
            bhvr.nested(current_element_->msg);
          } else {
            skipped = true;
          }
        }
      }
      ptr.swap(current_element_);
      if (skipped)
        push_to_cache(std::move(ptr));
    } while (skipped);
    if (! rcc.post())
      return;
  }
}

void blocking_actor::await_data() {
  if (! has_next_message())
    mailbox().synchronized_await(mtx_, cv_);
}

bool blocking_actor::await_data(timeout_type timeout) {
  if (has_next_message())
    return true;
  return mailbox().synchronized_await(mtx_, cv_, timeout);
}

size_t blocking_actor::attach_functor(const actor& x) {
  return attach_functor(actor_cast<strong_actor_ptr>(x));
}

size_t blocking_actor::attach_functor(const actor_addr& x) {
  return attach_functor(actor_cast<strong_actor_ptr>(x));
}

size_t blocking_actor::attach_functor(const strong_actor_ptr& ptr) {
  using wait_for_atom = atom_constant<atom("waitFor")>;
  if (! ptr)
    return 0;
  actor self{this};
  ptr->get()->attach_functor([=](const error&) {
    anon_send(self, wait_for_atom::value);
  });
  return 1;
}

} // namespace caf
