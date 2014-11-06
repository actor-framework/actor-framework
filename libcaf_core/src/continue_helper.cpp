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

#include "caf/continue_helper.hpp"
#include "caf/event_based_actor.hpp"

namespace caf {

continue_helper::continue_helper(message_id mid, getter g)
    : m_mid(mid), m_getter(std::move(g)) {}

continue_helper& continue_helper::continue_with(behavior::continuation_fun f) {
  auto ref_opt = m_getter(m_mid); //m_self->sync_handler(m_mid);
  if (ref_opt) {
    behavior cpy = *ref_opt;
    *ref_opt = cpy.add_continuation(std::move(f));
  } else {
    CAF_LOG_ERROR("failed to add continuation");
  }
  return *this;
}

} // namespace caf
