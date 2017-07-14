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

#ifndef CAF_RAW_EVENT_BASED_ACTOR_HPP
#define CAF_RAW_EVENT_BASED_ACTOR_HPP

#include "caf/event_based_actor.hpp"

namespace caf {

/// A cooperatively raw scheduled actor is a dynamically typed actor that does
/// not handle any system messages. All handler for system messages as well as
/// the default handler are ignored. This actor type is for testing and
/// system-level actors.
/// @extends event_based_actor
class raw_event_based_actor : public event_based_actor {
public:
  // -- member types -----------------------------------------------------------

  /// Required by `spawn` for type deduction.
  using signatures = none_t;

  /// Required by `spawn` for type deduction.
  using behavior_type = behavior;

  // -- constructors and destructors -------------------------------------------

  explicit raw_event_based_actor(actor_config& cfg);

  invoke_message_result consume(mailbox_element& x) override;
};

} // namespace caf

#endif // CAF_RAW_EVENT_BASED_ACTOR_HPP
