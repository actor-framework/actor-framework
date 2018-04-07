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
#include "caf/stream_slot.hpp"
#include "caf/unit.hpp"

#include "caf/intrusive/drr_queue.hpp"

namespace caf {
namespace policy {

/// Configures a dynamic WDRR queue for holding downstream messages.
class downstream_messages {
public:
  // -- nested types -----------------------------------------------------------

  /// Configures a nested DRR queue.
  class nested {
  public:
    // -- member types ---------------------------------------------------------

    using mapped_type = mailbox_element;

    using task_size_type = size_t;

    using deficit_type = size_t;

    using unique_pointer = mailbox_element_ptr;

    using handler_type = std::unique_ptr<inbound_path>;

    static task_size_type task_size(const mailbox_element& x) noexcept;

    // -- constructors, destructors, and assignment operators ------------------

    template <class T>
    nested(T&& x) : handler(std::forward<T>(x)) {
      // nop
    }

    nested() = default;

    nested(nested&&) = default;

    nested& operator=(nested&&) = default;

    nested(const nested&) = delete;

    nested& operator=(const nested&) = delete;

    // -- member variables -----------------------------------------------------

    handler_type handler;
  };

  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using unique_pointer = mailbox_element_ptr;

  using key_type = stream_slot;

  using nested_queue_type = intrusive::drr_queue<nested>;

  using queue_map_type = std::map<key_type, nested_queue_type>;

  // -- required functions for wdrr_dynamic_multiplexed_queue ------------------

  static key_type id_of(mailbox_element& x) noexcept;

  static bool enabled(const nested_queue_type& q) noexcept;

  static deficit_type quantum(const nested_queue_type& q,
                              deficit_type x) noexcept;

  // -- constructors, destructors, and assignment operators --------------------

  downstream_messages() = default;

  downstream_messages(const downstream_messages&) = default;

  downstream_messages& operator=(const downstream_messages&) = default;

  constexpr downstream_messages(unit_t) {
    // nop
  }

  // -- required functions for drr_queue ---------------------------------------

  static inline task_size_type task_size(const mailbox_element&) noexcept {
    return 1;
  }
};

} // namespace policy
} // namespace caf

