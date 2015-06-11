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

/*
 * Base type for typed and untyped event-based actors.
 * @tparam BehaviorType Denotes the expected type for become().
 * @tparam HasSyncSend Configures whether this base class extends `sync_sender`.
 * @extends local_actor
 */
template <class BehaviorType, bool HasSyncSend>
class abstract_event_based_actor : public
  std::conditional<
    HasSyncSend,
    extend<local_actor>
    ::with<mixin::sync_sender<nonblocking_response_handle_tag>::template impl>,
    local_actor
  >::type {
public:
  using behavior_type = BehaviorType;

  /****************************************************************************
   *                     become() member function family                      *
   ****************************************************************************/

  void become(behavior_type bhvr) {
    this->do_become(std::move(unbox(bhvr)), true);
  }

  void become(const keep_behavior_t&, behavior_type bhvr) {
    this->do_become(std::move(unbox(bhvr)), false);
  }

  template <class T, class... Ts>
  typename std::enable_if<
    ! std::is_same<keep_behavior_t, typename std::decay<T>::type>::value,
    void
  >::type
  become(T&& x, Ts&&... xs) {
    behavior_type bhvr{std::forward<T>(x), std::forward<Ts>(xs)...};
    this->do_become(std::move(unbox(bhvr)), true);
  }

  template <class... Ts>
  void become(const keep_behavior_t&, Ts&&... xs) {
    behavior_type bhvr{std::forward<Ts>(xs)...};
    this->do_become(std::move(unbox(bhvr)), false);
  }

  void unbecome() {
    this->bhvr_stack_.pop_back();
  }

private:
  template <class... Ts>
  static behavior& unbox(typed_behavior<Ts...>& x) {
    return x.unbox();
  }

  static inline behavior& unbox(behavior& x) {
    return x;
  }
};

} // namespace caf

#endif // CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP
