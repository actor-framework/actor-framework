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

#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/monitorable_actor.hpp"

namespace caf {
namespace decorator {

/// An actor decorator implementing "dot operator"-like compositions,
/// i.e., `f.g(x) = f(g(x))`. Composed actors are hidden actors.
/// A composed actor exits when either of its constituent actors exits;
/// Constituent actors have no dependency on the composed actor
/// by default, and exit of a composed actor has no effect on its
/// constituent actors. A composed actor is hosted on the same actor
/// system and node as `g`, the first actor on the forwarding chain.
class splitter : public monitorable_actor {
public:
  using message_types_set = std::set<std::string>;

  splitter(std::vector<strong_actor_ptr> workers, message_types_set msg_types);

  // non-system messages are processed and then forwarded;
  // system messages are handled and consumed on the spot;
  // in either case, the processing is done synchronously
  void enqueue(mailbox_element_ptr what, execution_unit* context) override;

  message_types_set message_types() const override;

private:
  const size_t num_workers;
  std::vector<strong_actor_ptr> workers_;
  message_types_set msg_types_;
};

} // namespace decorator
} // namespace caf

