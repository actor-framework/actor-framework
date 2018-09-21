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

#include "caf/none.hpp"
#include "caf/config.hpp"
#include "caf/make_counted.hpp"

#include "caf/logger.hpp"
#include "caf/detail/scope_guard.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace io {

void broker::initialize() {
  CAF_LOG_TRACE("");
  init_broker();
  auto bhvr = make_behavior();
  CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                          << CAF_ARG2("alive", alive()));
  if (bhvr) {
    // make_behavior() did return a behavior instead of using become()
    CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
    become(std::move(bhvr));
  }
}

broker::broker(actor_config& cfg) : super(cfg) {
  // nop
}


behavior broker::make_behavior() {
  behavior res;
  if (initial_behavior_fac_) {
    res = initial_behavior_fac_(this);
    initial_behavior_fac_ = nullptr;
  }
  return res;
}

} // namespace io
} // namespace caf
