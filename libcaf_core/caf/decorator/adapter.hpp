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

#ifndef CAF_DECORATOR_ADAPTER_HPP
#define CAF_DECORATOR_ADAPTER_HPP

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/attachable.hpp"
#include "caf/monitorable_actor.hpp"

namespace caf {
namespace decorator {

/// An actor decorator implementing `std::bind`-like compositions.
/// Bound actors are hidden actors. A bound actor exits when its
/// decorated actor exits. The decorated actor has no dependency
/// on the bound actor by default, and exit of a bound actor has
/// no effect on the decorated actor. Bound actors are hosted on
/// the same actor system and node as decorated actors.
class adapter : public monitorable_actor {
public:
  adapter(strong_actor_ptr decorated, message msg);

  // non-system messages are processed and then forwarded;
  // system messages are handled and consumed on the spot;
  // in either case, the processing is done synchronously
  void enqueue(mailbox_element_ptr what, execution_unit* host) override;

protected:
  void on_cleanup() override;

private:
  strong_actor_ptr decorated_;
  message merger_;
};

} // namespace decorator
} // namespace caf

#endif // CAF_DECORATOR_ADAPTER_HPP
