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
#include <algorithm>

#include "caf/response_promise.hpp"

#include "caf/logger.hpp"
#include "caf/local_actor.hpp"

namespace caf {

response_promise::response_promise() : self_(nullptr) {
  // nop
}

response_promise::response_promise(none_t) : response_promise() {
  // nop
}

response_promise::response_promise(strong_actor_ptr self, mailbox_element& src)
    : self_(std::move(self)),
      id_(src.mid) {
  // form an invalid request promise when initialized from a
  // response ID, since CAF always drops messages in this case
  if (!src.mid.is_response()) {
    source_ = std::move(src.sender);
    stages_ = std::move(src.stages);
  }
}

response_promise response_promise::deliver(error x) {
  //if (id_.valid())
  return deliver_impl(make_message(std::move(x)));
}

bool response_promise::async() const {
  return id_.is_async();
}


execution_unit* response_promise::context() {
  return self_ == nullptr
         ? nullptr
         : static_cast<local_actor*>(actor_cast<abstract_actor*>(self_))
           ->context();
}

response_promise response_promise::deliver_impl(message msg) {
  if (!stages_.empty()) {
    auto next = std::move(stages_.back());
    stages_.pop_back();
    next->enqueue(make_mailbox_element(std::move(source_), id_,
                                       std::move(stages_), std::move(msg)),
                  context());
    return *this;
  }
  if (source_) {
    source_->enqueue(std::move(self_), id_.response_id(),
                     std::move(msg), context());
    source_.reset();
    return *this;
  }
  if (self_)
    CAF_LOG_INFO("response promise already satisfied");
  else
    CAF_LOG_INFO("invalid response promise");
  return *this;
}

} // namespace caf
