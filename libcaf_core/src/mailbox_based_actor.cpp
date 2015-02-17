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

#include "caf/mailbox_based_actor.hpp"

namespace caf {

mailbox_based_actor::~mailbox_based_actor() {
  if (!m_mailbox.closed()) {
    detail::sync_request_bouncer f{this->exit_reason()};
    m_mailbox.close(f);
  }
}

void mailbox_based_actor::cleanup(uint32_t reason) {
  detail::sync_request_bouncer f{reason};
  m_mailbox.close(f);
  local_actor::cleanup(reason);
}

} // namespace caf
