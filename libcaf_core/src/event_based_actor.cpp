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

#include "caf/message_id.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/detail/pretty_type_name.hpp"

namespace caf {

event_based_actor::event_based_actor(actor_config& cfg) : extended_base(cfg) {
  // nop
}

event_based_actor::~event_based_actor() {
  // nop
}

void event_based_actor::initialize() {
  CAF_LOG_TRACE(CAF_ARG2("subtype",
                         detail::pretty_type_name(typeid(*this)).c_str()));
  extended_base::initialize();
  setf(is_initialized_flag);
  auto bhvr = make_behavior();
  CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                           << CAF_ARG2("alive", alive()));
  if (bhvr) {
    // make_behavior() did return a behavior instead of using become()
    CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
    become(std::move(bhvr));
  }
}

behavior event_based_actor::make_behavior() {
  CAF_LOG_TRACE("");
  behavior res;
  if (initial_behavior_fac_) {
    res = initial_behavior_fac_(this);
    initial_behavior_fac_ = nullptr;
  }
  return res;
}

} // namespace caf
