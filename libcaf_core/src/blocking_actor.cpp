/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include <utility>

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

blocking_actor::blocking_actor(actor_config& cfg)
    : extended_base(cfg.add_flag(local_actor::is_blocking_flag)) {
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
  // returns false if mailbox has been closed
  if (!mailbox().synchronized_enqueue(mtx_, cv_, ptr.release())) {
    CAF_LOG_REJECT_EVENT();
    if (mid.is_request()) {
      detail::sync_request_bouncer srb{exit_reason()};
      srb(src, mid);
    }
  } else {
    CAF_LOG_ACCEPT_EVENT();
  }
}

const char* blocking_actor::name() const {
  return "blocking_actor";
}

void blocking_actor::launch(execution_unit*, bool, bool hide) {
  CAF_LOG_TRACE(CAF_ARG(hide));
  CAF_ASSERT(getf(is_blocking_flag));
  if (!hide)
    register_at_system();
  home_system().inc_detached_threads();
  std::thread([](strong_actor_ptr ptr) {
    // actor lives in its own thread
    auto this_ptr = ptr->get();
    CAF_ASSERT(dynamic_cast<blocking_actor*>(this_ptr) != 0);
    auto self = static_cast<blocking_actor*>(this_ptr);
    CAF_SET_LOGGER_SYS(ptr->home_system);
    CAF_PUSH_AID_FROM_PTR(self);
    self->initialize();
    error rsn;
#   ifndef CAF_NO_EXCEPTIONS
    try {
      self->act();
      rsn = self->fail_state_;
    }
    catch (...) {
      rsn = exit_reason::unhandled_exception;
    }
    try {
      self->on_exit();
    }
    catch (...) {
      // simply ignore exception
    }
#   else
    self->act();
    rsn = self->fail_state_;
    self->on_exit();
#   endif
    self->cleanup(std::move(rsn), self->context());
    ptr->home_system->dec_detached_threads();
  }, strong_actor_ptr{ctrl()}).detach();
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

namespace {

class message_sequence {
public:
  virtual ~message_sequence() {
    // nop
  }
  virtual bool at_end() = 0;
  virtual bool await_value(bool reset_timeout) = 0;
  virtual mailbox_element& value() = 0;
  virtual void advance() = 0;
  virtual void erase_and_advance() = 0;
};

class cached_sequence : public message_sequence {
public:
  using cache_type = local_actor::mailbox_type::cache_type;
  using iterator = cache_type::iterator;

  cached_sequence(cache_type& cache)
      : cache_(cache),
        i_(cache.continuation()),
        e_(cache.end()) {
    // iterater to the first un-marked element
    i_ = advance_impl(i_);
  }

  bool at_end() override {
    return i_ == e_;
  }

  void advance() override {
    CAF_ASSERT(i_->marked);
    i_->marked = false;
    i_ = advance_impl(i_.next());
  }

  void erase_and_advance() override {
    CAF_ASSERT(i_->marked);
    i_ = advance_impl(cache_.erase(i_));
  }

  bool await_value(bool) override {
    return true;
  }

  mailbox_element& value() override {
    CAF_ASSERT(i_->marked);
    return *i_;
  }

public:
  iterator advance_impl(iterator i) {
    while (i != e_) {
      if (!i->marked) {
        i->marked = true;
        return i;
      }
      ++i;
    }
    return i;
  }

  cache_type& cache_;
  iterator i_;
  iterator e_;
};

class mailbox_sequence : public message_sequence {
public:
  mailbox_sequence(blocking_actor* self, duration rel_timeout)
      : self_(self),
        rel_tout_(rel_timeout) {
    next_timeout();

  }

  bool at_end() override {
    return false;
  }

  void advance() override {
    if (ptr_)
      self_->push_to_cache(std::move(ptr_));
    next_timeout();
  }

  void erase_and_advance() override {
    ptr_.reset();
    next_timeout();
  }

  bool await_value(bool reset_timeout) override {
    if (!rel_tout_.valid()) {
      self_->await_data();
      return true;
    }
    if (reset_timeout)
      next_timeout();
    return self_->await_data(abs_tout_);
  }

  mailbox_element& value() override {
    ptr_ = self_->dequeue();
    CAF_ASSERT(ptr_ != nullptr);
    return *ptr_;
  }

private:
  void next_timeout() {
    abs_tout_ = std::chrono::high_resolution_clock::now();
    abs_tout_ += rel_tout_;
  }

  blocking_actor* self_;
  duration rel_tout_;
  std::chrono::high_resolution_clock::time_point abs_tout_;
  mailbox_element_ptr ptr_;
};

class message_sequence_combinator : public message_sequence {
public:
  using pointer = message_sequence*;

  message_sequence_combinator(pointer first, pointer second)
      : ptr_(first),
        fallback_(second) {
    // nop
  }

  bool at_end() override {
    if (ptr_->at_end()) {
      if (fallback_ == nullptr)
        return true;
      ptr_ = fallback_;
      fallback_ = nullptr;
      return at_end();
    }
    return false;
  }

  void advance() override {
    ptr_->advance();
  }

  void erase_and_advance() override {
    ptr_->erase_and_advance();
  }

  bool await_value(bool reset_timeout) override {
    return ptr_->await_value(reset_timeout);
  }

  mailbox_element& value() override {
    return ptr_->value();
  }

private:
  pointer ptr_;
  pointer fallback_;
};

} // namespace <anonymous>

void blocking_actor::receive_impl(receive_cond& rcc,
                                  message_id mid,
                                  detail::blocking_behavior& bhvr) {
  CAF_LOG_TRACE(CAF_ARG(mid));
  // we start iterating the cache and iterating mailbox elements afterwards
  cached_sequence seq1{mailbox().cache()};
  mailbox_sequence seq2{this, bhvr.timeout()};
  message_sequence_combinator seq{&seq1, &seq2};
  detail::default_invoke_result_visitor<blocking_actor> visitor{this};
  // read incoming messages until we have a match or a timeout
  for (;;) {
    // check loop pre-condition
    if (!rcc.pre())
      return;
    // mailbox sequence is infinite, but at_end triggers the
    // transition from seq1 to seq2 if we iterated our cache
    if (seq.at_end())
      CAF_RAISE_ERROR("reached the end of an infinite sequence");
    // reset the timeout each iteration
    if (!seq.await_value(true)) {
      // short-circuit "loop body"
      bhvr.handle_timeout();
      if (!rcc.post())
        return;
      continue;
    }
    // skip messages in the loop body until we have a match
    bool skipped;
    bool timed_out;
    do {
      skipped = false;
      timed_out = false;
      auto& x = seq.value();
      CAF_LOG_RECEIVE_EVENT((&x));
      // skip messages that don't match our message ID
      if ((mid.valid() && mid != x.mid)
          || (!mid.valid() && x.mid.is_response())) {
        skipped = true;
        CAF_LOG_SKIP_EVENT();
      } else {
        // blocking actors can use nested receives => restore current_element_
        auto prev_element = current_element_;
        current_element_ = &x;
        switch (bhvr.nested(visitor, x.content())) {
          case match_case::skip:
            skipped = true;
            CAF_LOG_SKIP_EVENT();
            break;
          default:
            break;
          case match_case::no_match: {
            auto sres = bhvr.fallback(*current_element_);
            // when dealing with response messages, there's either a match
            // on the first handler or we produce an error to
            // get a match on the second (error) handler
            if (sres.flag != rt_skip) {
              visitor.visit(sres);
              CAF_LOG_FINALIZE_EVENT();
            } else if (mid.valid()) {
             // invoke again with an unexpected_response error
             auto& old = *current_element_;
             auto err = make_error(sec::unexpected_response,
                                   old.move_content_to_message());
             mailbox_element_view<error> tmp{std::move(old.sender), old.mid,
                                             std::move(old.stages), err};
             current_element_ = &tmp;
             bhvr.nested(tmp.content());
              CAF_LOG_FINALIZE_EVENT();
            } else {
              skipped = true;
              CAF_LOG_SKIP_EVENT();
            }
          }
        }
        current_element_ = prev_element;
      }
      if (skipped) {
        seq.advance();
        if (seq.at_end())
          CAF_RAISE_ERROR("reached the end of an infinite sequence");
        if (!seq.await_value(false)) {
          timed_out = true;
        }
      }
    } while (skipped && !timed_out);
    if (timed_out) {
      bhvr.handle_timeout();
      if (!rcc.post())
        return;
    } else {
      if (rcc.post())
        seq.erase_and_advance();
      else
        return;
    }
  }
}

void blocking_actor::await_data() {
  if (!has_next_message())
    mailbox().synchronized_await(mtx_, cv_);
}

bool blocking_actor::await_data(timeout_type timeout) {
  if (has_next_message())
    return true;
  return mailbox().synchronized_await(mtx_, cv_, timeout);
}

mailbox_element_ptr blocking_actor::dequeue() {
  return next_message();
}

void blocking_actor::varargs_tup_receive(receive_cond& rcc, message_id mid,
                                         std::tuple<behavior&>& tup) {
  using namespace detail;
  auto& bhvr = std::get<0>(tup);
  if (bhvr.timeout().valid()) {
    auto tmp = after(bhvr.timeout()) >> [&] {
      bhvr.handle_timeout();
    };
    auto fun = make_blocking_behavior(&bhvr, std::move(tmp));
    receive_impl(rcc, mid, fun);
  } else {
    auto fun = make_blocking_behavior(&bhvr);
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
  using wait_for_atom = atom_constant<atom("waitFor")>;
  if (!ptr)
    return 0;
  actor self{this};
  ptr->get()->attach_functor([=](const error&) {
    anon_send(self, wait_for_atom::value);
  });
  return 1;
}

} // namespace caf
