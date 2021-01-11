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

response_promise::response_promise() : self_(nullptr) {
  // nop
}

response_promise::response_promise(none_t) : response_promise() {
  // nop
}

response_promise::response_promise(strong_actor_ptr self,
                                   strong_actor_ptr source,
                                   forwarding_stack stages, message_id mid)
  : id_(mid) {
  CAF_ASSERT(self != nullptr);
  // Form an invalid request promise when initialized from a response ID, since
  // we always drop messages in this case.
  if (!mid.is_response()) {
    self_.swap(self);
    source_.swap(source);
    stages_.swap(stages);
  }
}

response_promise::response_promise(strong_actor_ptr self, mailbox_element& src)
  : response_promise(std::move(self), std::move(src.sender),
                     std::move(src.stages), src.mid) {
  // nop
}

void response_promise::deliver(error x) {
  deliver_impl(make_message(std::move(x)));
}

void response_promise::deliver() {
  deliver_impl(make_message());
}

bool response_promise::async() const {
  return id_.is_async();
}

local_actor* response_promise::self_dptr() const {
  // TODO: We require that self_ was constructed by using a local_actor*. The
  //       type erasure performed by strong_actor_ptr hides that fact. We
  //       probably should use a different pointer type such as
  //       intrusive_ptr<local_actor>. However, that would mean we would have to
  //       include local_actor.hpp in response_promise.hpp or provide overloads
  //       for intrusive_ptr_add_ref and intrusive_ptr_release.
  auto self_baseptr = actor_cast<abstract_actor*>(self_);
  return static_cast<local_actor*>(self_baseptr);
}

execution_unit* response_promise::context() {
  return self_ == nullptr ? nullptr : self_dptr()->context();
}

void response_promise::deliver_impl(message msg) {
  CAF_LOG_TRACE(CAF_ARG(msg));
  if (self_ == nullptr) {
    CAF_LOG_DEBUG("drop response: invalid promise");
    return;
  }
  if (msg.empty() && id_.is_async()) {
    CAF_LOG_DEBUG("drop response: empty response to asynchronous input");
    self_.reset();
    return;
  }
  auto dptr = self_dptr();
  if (!stages_.empty()) {
    auto next = std::move(stages_.back());
    stages_.pop_back();
    detail::profiled_send(dptr, std::move(source_), next, id_,
                          std::move(stages_), dptr->context(), std::move(msg));
    self_.reset();
    return;
  }
  if (source_) {
    detail::profiled_send(dptr, self_, source_, id_.response_id(), no_stages,
                          dptr->context(), std::move(msg));
    self_.reset();
    source_.reset();
    return;
  }
  CAF_LOG_WARNING("malformed response promise: self != nullptr && !pending()");
}

void response_promise::delegate_impl(abstract_actor* receiver, message msg) {
  CAF_LOG_TRACE(CAF_ARG(msg));
  if (receiver == nullptr) {
    CAF_LOG_DEBUG("drop response: invalid delegation target");
    return;
  }
  if (self_ == nullptr) {
    CAF_LOG_DEBUG("drop response: invalid promise");
    return;
  }
  auto dptr = self_dptr();
  detail::profiled_send(dptr, std::move(source_), receiver, id_,
                        std::move(stages_), dptr->context(), std::move(msg));
  self_.reset();
}

} // namespace caf
