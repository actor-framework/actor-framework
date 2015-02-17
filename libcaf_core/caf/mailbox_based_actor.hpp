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

#ifndef CAF_MAILBOX_BASED_ACTOR_HPP
#define CAF_MAILBOX_BASED_ACTOR_HPP

#include <type_traits>

#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {

/**
 * Base class for local running actors using a mailbox.
 */
class mailbox_based_actor : public local_actor {
 public:
  using del = detail::disposer;
  using mailbox_type = detail::single_reader_queue<mailbox_element, del>;

  ~mailbox_based_actor();

  void cleanup(uint32_t reason);

  inline mailbox_type& mailbox() {
    return m_mailbox;
  }

 protected:
  mailbox_type m_mailbox;
};

} // namespace caf

#endif // CAF_MAILBOX_BASED_ACTOR_HPP
