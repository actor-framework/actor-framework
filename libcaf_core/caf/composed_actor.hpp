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

#ifndef CAF_COMPOSED_ACTOR_HPP
#define CAF_COMPOSED_ACTOR_HPP

#include "caf/local_actor.hpp"

namespace caf {

/// An actor decorator implementing "dot operator"-like compositions,
/// i.e., `f.g(x) = f(g(x))`.
class composed_actor : public local_actor {
public:
  composed_actor(actor_system* sys, actor_addr first, actor_addr second);

  void initialize() override;

  void enqueue(mailbox_element_ptr what, execution_unit* host) override;

private:
  bool is_system_message(const message& msg);

  actor_addr first_;
  actor_addr second_;
};

} // namespace caf

#endif // CAF_COMPOSED_ACTOR_HPP
