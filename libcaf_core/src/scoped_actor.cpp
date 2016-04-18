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
#include "caf/actor_registry.hpp"
#include "caf/scoped_execution_unit.hpp"

namespace caf {

namespace {

class impl : public blocking_actor {
public:
  impl(actor_config& cfg) : blocking_actor(cfg) {
    is_detached(true);
  }

  void act() override {
    CAF_LOG_ERROR("act() of scoped_actor impl called");
  }

  const char* name() const override {
    return "scoped_actor";
  }
};

} // namespace <anonymous>

scoped_actor::scoped_actor(actor_system& sys, bool hidden) : context_{&sys} {
  actor_config cfg{&context_};
  self_ = make_actor<impl, strong_actor_ptr>(sys.next_actor_id(), sys.node(),
                                             &sys, cfg);
  if (! hidden)
    prev_ = CAF_SET_AID(self_->id());
  CAF_LOG_TRACE(CAF_ARG(hidden));
  ptr()->is_registered(! hidden);
}

scoped_actor::~scoped_actor() {
  CAF_LOG_TRACE("");
  if (! self_)
    return;
  if (ptr()->is_registered()) {
    CAF_SET_AID(prev_);
  }
  auto r = ptr()->planned_exit_reason();
  ptr()->cleanup(r == exit_reason::not_exited ? exit_reason::normal : r,
                     &context_);
}

blocking_actor* scoped_actor::ptr() const {
  return static_cast<blocking_actor*>(actor_cast<abstract_actor*>(self_));
}

std::string to_string(const scoped_actor& x) {
  return to_string(x.address());
}

} // namespace caf
