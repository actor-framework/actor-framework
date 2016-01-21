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

#include <utility>
#include <algorithm>

#include "caf/local_actor.hpp"
#include "caf/response_promise.hpp"

namespace caf {

response_promise::response_promise()
    : self_(nullptr) {
  // nop
}

response_promise::response_promise(local_actor* self, mailbox_element& src)
    : self_(self),
      source_(std::move(src.sender)),
      stages_(std::move(src.stages)),
      id_(src.mid) {
  src.mid.mark_as_answered();
}

void response_promise::deliver(error x) {
  if (id_.valid())
    deliver_impl(make_message(std::move(x)));
}

void response_promise::deliver_impl(message msg) {
  if (! stages_.empty()) {
    auto next = std::move(stages_.back());
    stages_.pop_back();
    next->enqueue(mailbox_element::make(std::move(source_), id_,
                                        std::move(stages_), std::move(msg)),
                  self_->context());
    return;
  }
  if (source_) {
    source_->enqueue(self_->address(), id_.response_id(),
                     std::move(msg), self_->context());
    source_ = invalid_actor_addr;
    return;
  }
  if (self_)
    CAF_LOG_ERROR("response promise already satisfied");
  else
    CAF_LOG_ERROR("invalid response promise");
}

} // namespace caf
