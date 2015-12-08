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
#include "caf/abstract_actor.hpp"

namespace caf {

/// An actor decorator implementing `std::bind`-like compositions.
class bound_actor : public abstract_actor {
public:
  bound_actor(actor_system* sys, actor_addr decorated, message msg);

  void attach(attachable_ptr ptr) override;

  size_t detach(const attachable::token& what) override;

  void enqueue(mailbox_element_ptr what, execution_unit* host) override;

protected:
  bool link_impl(linking_operation op, const actor_addr& other) override;

private:
  actor_addr decorated_;
  message merger_;
};

} // namespace caf

#endif // CAF_BOUND_ACTOR_HPP
