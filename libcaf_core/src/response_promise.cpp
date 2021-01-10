// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include <algorithm>
#include <utility>

#include "caf/response_promise.hpp"

#include "caf/detail/profiled_send.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

response_promise::response_promise(strong_actor_ptr self,
                                   strong_actor_ptr source,
                                   forwarding_stack stages, message_id mid) {
  CAF_ASSERT(self != nullptr);
  // Form an invalid request promise when initialized from a response ID, since
  // we always drop messages in this case. Also don't create promises for
  // anonymous messages since there's nowhere to send the message to anyway.
  if (!mid.is_response() && (source || !stages.empty())) {
    state_ = make_counted<state>();
    state_->self.swap(self);
    state_->source.swap(source);
    state_->stages.swap(stages);
    state_->id = mid;
  }
}

response_promise::response_promise(strong_actor_ptr self, mailbox_element& src)
  : response_promise(std::move(self), std::move(src.sender),
                     std::move(src.stages), src.mid) {
  // nop
}

// -- properties ---------------------------------------------------------------

bool response_promise::async() const noexcept {
  return id().is_async();
}

bool response_promise::pending() const noexcept {
  return state_ != nullptr && state_->self != nullptr;
}

strong_actor_ptr response_promise::source() const noexcept {
  if (state_)
    return state_->source;
  else
    return nullptr;
}

response_promise::forwarding_stack response_promise::stages() const {
  if (state_)
    return state_->stages;
  else
    return {};
}

strong_actor_ptr response_promise::next() const noexcept {
  if (state_)
    return state_->stages.empty() ? state_->source : state_->stages[0];
  else
    return nullptr;
}

message_id response_promise::id() const noexcept {
  if (state_)
    return state_->id;
  else
    return make_message_id();
}

// -- delivery -----------------------------------------------------------------

void response_promise::deliver(message msg) {
  CAF_LOG_TRACE(CAF_ARG(msg));
  if (pending()) {
    state_->deliver_impl(std::move(msg));
    state_.reset();
  }
}

void response_promise::deliver(error x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (pending()) {
    state_->deliver_impl(make_message(std::move(x)));
    state_.reset();
  }
}

void response_promise::deliver() {
  CAF_LOG_TRACE(CAF_ARG(""));
  if (pending()) {
    state_->deliver_impl(make_message());
    state_.reset();
  }
}

// -- state --------------------------------------------------------------------

response_promise::state::~state() {
  if (self) {
    CAF_LOG_DEBUG("broken promise!");
    deliver_impl(make_message(make_error(sec::broken_promise)));
  }
}

void response_promise::state::cancel() {
  self.reset();
}

void response_promise::state::deliver_impl(message msg) {
  CAF_LOG_TRACE(CAF_ARG(msg));
  if (msg.empty() && id.is_async()) {
    CAF_LOG_DEBUG("drop response: empty response to asynchronous input");
  } else {
    auto dptr = actor_cast<local_actor*>(self);
    if (stages.empty()) {
      detail::profiled_send(dptr, self, source, id.response_id(),
                            forwarding_stack{}, dptr->context(),
                            std::move(msg));
    } else {
      auto next = std::move(stages.back());
      stages.pop_back();
      detail::profiled_send(dptr, std::move(source), next, id,
                            std::move(stages), dptr->context(), std::move(msg));
    }
  }
  cancel();
}

void response_promise::state::delegate_impl(abstract_actor* receiver,
                                            message msg) {
  CAF_LOG_TRACE(CAF_ARG(msg));
  if (receiver != nullptr) {
    auto dptr = actor_cast<local_actor*>(self);
    detail::profiled_send(dptr, std::move(source), receiver, id,
                          std::move(stages), dptr->context(), std::move(msg));
  } else {
    CAF_LOG_DEBUG("drop response: invalid delegation target");
  }
  cancel();
}

} // namespace caf
