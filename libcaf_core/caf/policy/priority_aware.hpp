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

#pragma once

#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_priority.hpp"
#include "caf/unit.hpp"

#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"

namespace caf {
namespace policy {

/// Configures a cached WDRR fixed multiplexed queue for dispatching to two
/// nested queue (one for each message priority).
class priority_aware {
public:
  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using unique_pointer = mailbox_element_ptr;

  // -- constructors, destructors, and assignment operators --------------------

  priority_aware() = default;

  priority_aware(const priority_aware&) = default;

  priority_aware& operator=(const priority_aware&) = default;

  constexpr priority_aware(unit_t) {
    // nop
  }

  // -- interface required by wdrr_fixed_multiplexed_queue ---------------------

  template <template <class> class Queue>
  static deficit_type quantum(const Queue<urgent_messages>&,
                              deficit_type x) noexcept {
    return x * static_cast<deficit_type>(message_priority::high);
  }

  template <class Queue>
  static deficit_type quantum(const Queue&, deficit_type x) noexcept {
    return x;
  }

  static inline size_t id_of(const mailbox_element& x) noexcept {
    return x.mid.category() != message_id::urgent_message_category ? 0u : 1u;
  }
};

} // namespace policy
} // namespace caf

