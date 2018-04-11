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

#include "caf/policy/downstream_messages.hpp"

#include "caf/downstream_msg.hpp"
#include "caf/inbound_path.hpp"
#include "caf/logger.hpp"

namespace caf {
namespace policy {

namespace {

class task_size_calculator {
public:
  using size_type = downstream_messages::nested::task_size_type;

  inline size_type operator()(const downstream_msg::batch& x) const noexcept {
    CAF_ASSERT(x.xs_size > 0);
    return static_cast<size_type>(x.xs_size);
  }

  template <class T>
  size_type operator()(const T&) const noexcept {
    return 1;
  }
};

} // namespace <anonymous>

auto downstream_messages::nested::task_size(const mailbox_element& x) noexcept
  -> task_size_type {
  task_size_calculator f;
  return visit(f, x.content().get_as<downstream_msg>(0).content);
}

auto downstream_messages::id_of(mailbox_element& x) noexcept -> key_type {
  return x.content().get_as<downstream_msg>(0).slots.receiver;
}

bool downstream_messages::enabled(const nested_queue_type& q) noexcept {
  auto congested = q.policy().handler->mgr->congested();
  CAF_LOG_DEBUG_IF(congested, "path is congested:"
                   << CAF_ARG2( "slot", q.policy().handler->slots.receiver));
  return !congested;
}

auto downstream_messages::quantum(const nested_queue_type& q,
                                  deficit_type x) noexcept -> deficit_type {
  // TODO: adjust quantum based on the stream priority
  return x * static_cast<deficit_type>(q.policy().handler->desired_batch_size);
}

} // namespace policy
} // namespace caf

