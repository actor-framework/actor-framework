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

#ifndef CAF_EVENT_BASED_ACTOR_HPP
#define CAF_EVENT_BASED_ACTOR_HPP

#include <type_traits>

#include "caf/on.hpp"
#include "caf/extend.hpp"
#include "caf/local_actor.hpp"
#include "caf/response_handle.hpp"
#include "caf/abstract_event_based_actor.hpp"

#include "caf/detail/logging.hpp"

namespace caf {

/// A cooperatively scheduled, event-based actor implementation. This is the
/// recommended base class for user-defined actors and is used implicitly when
/// spawning functor-based actors without the `blocking_api` flag.
/// @extends local_actor
class event_based_actor : public abstract_event_based_actor<behavior, true> {
public:
  /// Forwards the last received message to `whom`.
  void forward_to(const actor& whom,
                  message_priority = message_priority::normal);

  event_based_actor() = default;

  ~event_based_actor();

  void initialize() override;

protected:
  /// Returns the initial actor behavior.
  virtual behavior make_behavior();
};

} // namespace caf

#endif // CAF_EVENT_BASED_ACTOR_HPP
