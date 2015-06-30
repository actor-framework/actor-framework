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

#ifndef CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP
#define CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP

#include <type_traits>

#include "caf/message_id.hpp"
#include "caf/local_actor.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/behavior_policy.hpp"
#include "caf/response_handle.hpp"

#include "caf/mixin/sync_sender.hpp"

namespace caf {

/// Base type for statically and dynamically typed event-based actors.
/// @tparam BehaviorType Denotes the expected type for become().
/// @tparam HasSyncSend Configures whether this base extends `sync_sender`.
/// @tparam Base Either `local_actor` (default) or a subtype thereof.
template <class BehaviorType, bool HasSyncSend, class Base = local_actor>
class abstract_event_based_actor : public Base {
public:
  using behavior_type = BehaviorType;

  template <class... Ts>
  abstract_event_based_actor(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  /****************************************************************************
   *                     become() member function family                      *
   ****************************************************************************/

  void become(behavior_type bhvr) {
    this->do_become(std::move(bhvr.unbox()), true);
  }

  void become(const keep_behavior_t&, behavior_type bhvr) {
    this->do_become(std::move(bhvr.unbox()), false);
  }

  template <class T0, class T1, class... Ts>
  typename std::enable_if<
    ! std::is_same<keep_behavior_t, typename std::decay<T0>::type>::value
  >::type
  become(T0&& x0, T1&& x1, Ts&&... xs) {
    behavior_type bhvr{std::forward<T0>(x0),
                       std::forward<T1>(x1),
                       std::forward<Ts>(xs)...};
    this->do_become(std::move(bhvr.unbox()), true);
  }

  template <class T0, class T1, class... Ts>
  void become(const keep_behavior_t&, T0&& x0, T1&& x1, Ts&&... xs) {
    behavior_type bhvr{std::forward<T0>(x0),
                       std::forward<T1>(x1),
                       std::forward<Ts>(xs)...};
    this->do_become(std::move(bhvr.unbox()), false);
  }

  void unbecome() {
    this->bhvr_stack_.pop_back();
  }
};

template <class BehaviorType, class Base>
class abstract_event_based_actor<BehaviorType, true, Base>
  : public extend<abstract_event_based_actor<BehaviorType, false, Base>>
           ::template with<mixin::sync_sender<nonblocking_response_handle_tag>
                           ::template impl> {
public:
  using super
    = typename extend<abstract_event_based_actor<BehaviorType, false, Base>>
      ::template with<mixin::sync_sender<nonblocking_response_handle_tag>
                      ::template impl>;

  template <class... Ts>
  abstract_event_based_actor(Ts&&... xs) : super(std::forward<Ts>(xs)...) {
    // nop
  }
};

} // namespace caf

#endif // CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP
