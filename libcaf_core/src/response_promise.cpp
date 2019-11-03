/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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

void response_promise::deliver(unit_t) {
  deliver_impl(make_message());
}

bool response_promise::async() const {
  return id_.is_async();
}

local_actor* response_promise::self_ptr() const {
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
  return self_ == nullptr ? nullptr : self_ptr()->context();
}

void response_promise::deliver_impl(message msg) {
  CAF_LOG_TRACE(CAF_ARG(msg));
  if (self_ == nullptr) {
    CAF_LOG_DEBUG("drop response: invalid promise");
    return;
  }
  auto self = self_ptr();
  if (!stages_.empty()) {
    auto next = std::move(stages_.back());
    stages_.pop_back();
    detail::profiled_send(self, std::move(source_), next, id_,
                          std::move(stages_), self->context(), std::move(msg));
    self_.reset();
    return;
  }
  if (source_) {
    detail::profiled_send(self, self_, source_, id_.response_id(), no_stages,
                          self->context(), std::move(msg));
    self_.reset();
    source_.reset();
    return;
  }
  CAF_LOG_WARNING("malformed response promise: self != nullptr && !pending()");
}

} // namespace caf
