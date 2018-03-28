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
#include "caf/unit.hpp"

namespace caf {
namespace policy {

/// Configures a cached DRR queue for holding asynchronous messages with
/// default priority.
class urgent_messages {
public:
  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using unique_pointer = mailbox_element_ptr;

  // -- constructors, destructors, and assignment operators --------------------

  urgent_messages() = default;

  urgent_messages(const urgent_messages&) = default;

  urgent_messages& operator=(const urgent_messages&) = default;

  constexpr urgent_messages(unit_t) {
    // nop
  }

  // -- interface required by drr_queue ----------------------------------------

  static inline task_size_type task_size(const mailbox_element&) noexcept {
    return 1;
  }
};

} // namespace policy
} // namespace caf

