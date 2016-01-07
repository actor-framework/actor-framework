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

#ifndef CAF_BOUND_ACTOR_HPP
#define CAF_BOUND_ACTOR_HPP

#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/monitorable_actor.hpp"

namespace caf {

/// An actor decorator implementing `std::bind`-like compositions.
/// Bound actors are hidden actors. A bound actor exits when its
/// decorated actor exits. The decorated actor has no dependency
/// on the bound actor by default, and exit of a bound actor has
/// no effect on the decorated actor. Bound actors are hosted on
/// the same actor system and node as decorated actors.
class bound_actor : public monitorable_actor {
public:
  bound_actor(actor_addr decorated, message msg);

  // non-system messages are processed and then forwarded;
  // system messages are handled and consumed on the spot;
  // in either case, the processing is done synchronously
  void enqueue(mailbox_element_ptr what, execution_unit* host) override;

private:
  actor_addr decorated_;
  message merger_;
};

} // namespace caf

#endif // CAF_BOUND_ACTOR_HPP
