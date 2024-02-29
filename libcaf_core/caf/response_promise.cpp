// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/response_promise.hpp"

#include "caf/detail/profiled_send.hpp"
#include "caf/local_actor.hpp"
#include "caf/log/core.hpp"

#include <algorithm>
#include <utility>

namespace caf {

namespace {

bool requires_response(message_id mid) {
  return !mid.is_response() && !mid.is_answered();
}

bool requires_response(const mailbox_element& src) {
  return requires_response(src.mid);
}

bool has_response_receiver(const mailbox_element& src) {
  return src.sender != nullptr;
}

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

response_promise::response_promise(local_actor* self, strong_actor_ptr source,
                                   message_id mid) {
  CAF_ASSERT(self != nullptr);
  // Form an invalid request promise when initialized from a response ID, since
  // we always drop messages in this case. Also don't create promises for
  // anonymous messages since there's nowhere to send the message to anyway.
  if (requires_response(mid)) {
    state_ = make_counted<state>();
    state_->self = self->ctrl();
    state_->source.swap(source);
    state_->id = mid;
  }
}

response_promise::response_promise(local_actor* self, mailbox_element& src)
  : response_promise(self, std::move(src.sender), src.mid) {
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

message_id response_promise::id() const noexcept {
  if (state_)
    return state_->id;
  else
    return make_message_id();
}

// -- delivery -----------------------------------------------------------------

void response_promise::deliver(message msg) {
  auto lg = log::core::trace("msg = {}", msg);
  if (pending()) {
    state_->deliver_impl(std::move(msg));
    state_.reset();
  }
}

void response_promise::deliver(error x) {
  auto lg = log::core::trace("x = {}", x);
  if (pending()) {
    state_->deliver_impl(make_message(std::move(x)));
    state_.reset();
  }
}

void response_promise::deliver() {
  auto lg = log::core::trace("");
  if (pending()) {
    state_->deliver_impl(make_message());
    state_.reset();
  }
}

void response_promise::respond_to(local_actor* self, mailbox_element* request,
                                  message& response) {
  if (request && requires_response(*request)
      && has_response_receiver(*request)) {
    state tmp;
    tmp.self = self->ctrl();
    tmp.source.swap(request->sender);
    tmp.id = request->mid;
    tmp.deliver_impl(std::move(response));
    request->mid.mark_as_answered();
  }
}

void response_promise::respond_to(local_actor* self, mailbox_element* request,
                                  error& response) {
  if (request && requires_response(*request)
      && has_response_receiver(*request)) {
    state tmp;
    tmp.self = self->ctrl();
    tmp.source.swap(request->sender);
    tmp.id = request->mid;
    tmp.deliver_impl(make_message(std::move(response)));
    request->mid.mark_as_answered();
  }
}

// -- state --------------------------------------------------------------------

response_promise::state::~state() {
  // Note: the state may get destroyed outside of the actor. For example, when
  //       storing the promise in a run-later continuation. Hence, we can't call
  //       deliver_impl here since it calls self->context().
  if (self && source) {
    log::core::debug("broken promise!");
    auto element = make_mailbox_element(self, id.response_id(),
                                        make_error(sec::broken_promise));
    source->enqueue(std::move(element), nullptr);
  }
}

void response_promise::state::cancel() {
  self = nullptr;
}

void response_promise::state::deliver_impl(message msg) {
  auto lg = log::core::trace("msg = {}", msg);
  // Even though we are holding a weak pointer, we can access the pointer
  // without any additional check here because only the actor itself is allowed
  // to call this function.
  auto selfptr = static_cast<local_actor*>(self->get());
  if (msg.empty() && id.is_async()) {
    log::core::debug("drop response: empty response to asynchronous input");
  } else if (source != nullptr) {
    detail::profiled_send(selfptr, self, source, id.response_id(),
                          selfptr->context(), std::move(msg));
  }
  cancel();
}

void response_promise::state::delegate_impl(abstract_actor* receiver,
                                            message msg) {
  auto lg = log::core::trace("msg = {}", msg);
  if (receiver != nullptr) {
    auto selfptr = static_cast<local_actor*>(self->get());
    detail::profiled_send(selfptr, std::move(source), receiver, id,
                          selfptr->context(), std::move(msg));
  } else {
    log::core::debug("drop response: invalid delegation target");
  }
  cancel();
}

} // namespace caf
