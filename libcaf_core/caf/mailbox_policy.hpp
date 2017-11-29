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

#ifndef CAF_MAILBOX_POLICY_HPP
#define CAF_MAILBOX_POLICY_HPP

#include "caf/mailbox_element.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/drr_cached_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"

namespace caf {

/// Configures a mailbox queue containing four nested queues.
class mailbox_policy {
public:
  // -- nested types -----------------------------------------------------------

  class default_queue;

  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using key_type = size_t;

  using task_size_type = long;

  using deficit_type = long;

  using deleter_type = detail::disposer;

  using unique_pointer = mailbox_element_ptr;

  using stream_queue = intrusive::drr_queue<mailbox_policy>;

  using high_priority_queue = intrusive::drr_cached_queue<mailbox_policy>;

  using queue_type =
    intrusive::wdrr_fixed_multiplexed_queue<mailbox_policy, default_queue,
                                            stream_queue,
                                            stream_queue,
                                            high_priority_queue>;

  static constexpr size_t default_queue_index = 0;

  static constexpr size_t high_priority_queue_index = 3;

  static inline key_type id_of(const mapped_type& x) noexcept {
    return static_cast<key_type>(x.mid.category());
  }

  static inline task_size_type task_size(const mapped_type&) noexcept {
    return 1;
  }

  static inline deficit_type quantum(const default_queue&,
                                     deficit_type x) noexcept {
    return x;
  }

  static inline deficit_type quantum(const stream_queue&,
                                     deficit_type x) noexcept {
    return x;
  }

  /// Handle 5 high priority messages for each default messages.
  static inline deficit_type quantum(const high_priority_queue&,
                                     deficit_type x) noexcept {
    return x * 5;
  }
};

class mailbox_policy::default_queue
    : public intrusive::drr_cached_queue<mailbox_policy> {
public:
  using super = drr_cached_queue<mailbox_policy>;

  using super::super;
};

} // namespace caf

#endif // CAF_MAILBOX_POLICY_HPP
