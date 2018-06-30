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

#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/extend.hpp"
#include "caf/local_actor.hpp"
#include "caf/actor_marker.hpp"
#include "caf/response_handle.hpp"
#include "caf/scheduled_actor.hpp"

#include "caf/mixin/sender.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/subscriber.hpp"
#include "caf/mixin/behavior_changer.hpp"

#include "caf/logger.hpp"

namespace caf {

template <>
class behavior_type_of<event_based_actor> {
public:
  using type = behavior;
};

/// A cooperatively scheduled, event-based actor implementation. This is the
/// recommended base class for user-defined actors.
/// @extends scheduled_actor
class event_based_actor : public extend<scheduled_actor, event_based_actor>::
                                 with<mixin::sender,
                                      mixin::requester,
                                      mixin::subscriber,
                                      mixin::behavior_changer>,
                          public dynamically_typed_actor_base {
public:
  // -- member types -----------------------------------------------------------

  /// Required by `spawn` for type deduction.
  using signatures = none_t;

  /// Required by `spawn` for type deduction.
  using behavior_type = behavior;

  // -- constructors, destructors ----------------------------------------------

  explicit event_based_actor(actor_config& cfg);

  ~event_based_actor() override;

  // -- overridden functions of local_actor ------------------------------------

  void initialize() override;

protected:
  // -- behavior management ----------------------------------------------------

  /// Returns the initial actor behavior.
  virtual behavior make_behavior();
};

} // namespace caf

