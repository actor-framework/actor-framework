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

#pragma once

#include <cstdint>

#include "caf/error.hpp"
#include "caf/fwd.hpp"

#include "caf/intrusive/task_result.hpp"

namespace caf {
namespace detail {

/// Drains a mailbox and sends an error message to each unhandled request.
struct sync_request_bouncer {
  error rsn;
  explicit sync_request_bouncer(error r);
  void operator()(const strong_actor_ptr& sender, const message_id& mid) const;
  void operator()(const mailbox_element& e) const;

  /// Unwrap WDRR queues. Nesting WDRR queues results in a Key/Queue prefix for
  /// each layer of nesting.
  template <class Key, class Queue, class... Ts>
  intrusive::task_result operator()(const Key&, const Queue&,
                                    const Ts&... xs) const {
    (*this)(xs...);
    return intrusive::task_result::resume;
  }
};

} // namespace detail
} // namespace caf

