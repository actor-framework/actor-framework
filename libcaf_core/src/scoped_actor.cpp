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

#include "caf/scoped_actor.hpp"

#include "caf/spawn_options.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {

namespace {

struct impl : blocking_actor {
  impl() {
    is_detached(true);
  }

  void act() override {
    CAF_LOG_ERROR("act() of scoped_actor impl called");
  }
};

} // namespace <anonymous>

void scoped_actor::init(bool hide_actor) {
  self_.reset(new impl, false);
  if (! hide_actor) {
    prev_ = CAF_SET_AID(self_->id());
  }
  CAF_LOG_TRACE(CAF_ARG(hide_actor));
  self_->is_registered(! hide_actor);
}

scoped_actor::scoped_actor() {
  init(false);
}

scoped_actor::scoped_actor(bool hide_actor) {
  init(hide_actor);
}

scoped_actor::~scoped_actor() {
  CAF_LOG_TRACE("");
  if (self_->is_registered()) {
    CAF_SET_AID(prev_);
  }
  auto r = self_->planned_exit_reason();
  self_->cleanup(r == exit_reason::not_exited ? exit_reason::normal : r);
}

} // namespace caf
