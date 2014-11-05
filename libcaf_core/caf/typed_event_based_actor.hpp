/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_TYPED_EVENT_BASED_ACTOR_HPP
#define CAF_TYPED_EVENT_BASED_ACTOR_HPP

#include "caf/replies_to.hpp"
#include "caf/local_actor.hpp"
#include "caf/typed_behavior.hpp"

#include "caf/mixin/sync_sender.hpp"
#include "caf/mixin/mailbox_based.hpp"
#include "caf/mixin/behavior_stack_based.hpp"

namespace caf {

/**
 * A cooperatively scheduled, event-based actor implementation with strong type
 * checking. This is the recommended base class for user-defined actors and is
 * used implicitly when spawning typed, functor-based actors without the
 * `blocking_api` flag.
 * @extends local_actor
 */
template <class... Rs>
class typed_event_based_actor : public
    extend<local_actor, typed_event_based_actor<Rs...>>::template
    with<mixin::mailbox_based,
         mixin::behavior_stack_based<typed_behavior<Rs...>>::template impl,
         mixin::sync_sender<nonblocking_response_handle_tag>::template impl> {
 public:
  using signatures = detail::type_list<Rs...>;

  using behavior_type = typed_behavior<Rs...>;

  std::set<std::string> message_types() const override {
    return {detail::to_uniform_name<Rs>()...};
  }

 protected:
  virtual behavior_type make_behavior() = 0;
};

} // namespace caf

#endif // CAF_TYPED_EVENT_BASED_ACTOR_HPP
