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

#ifndef CAF_THREADLESS_HPP
#define CAF_THREADLESS_HPP

#include "caf/atom.hpp"
#include "caf/behavior.hpp"
#include "caf/duration.hpp"

#include "caf/policy/invoke_policy.hpp"

namespace caf {
namespace policy {

/**
 * An actor that is scheduled or otherwise managed.
 */
class sequential_invoke : public invoke_policy<sequential_invoke> {

 public:

  inline bool hm_should_skip(mailbox_element*) { return false; }

  template <class Actor>
  inline mailbox_element* hm_begin(Actor* self, mailbox_element* node) {
    auto previous = self->current_node();
    self->current_node(node);
    return previous;
  }

  template <class Actor>
  inline void hm_cleanup(Actor* self, mailbox_element*) {
    self->current_node(self->dummy_node());
  }

  template <class Actor>
  inline void hm_revert(Actor* self, mailbox_element* previous) {
    self->current_node(previous);
  }

};

} // namespace policy
} // namespace caf

#endif // CAF_THREADLESS_HPP
