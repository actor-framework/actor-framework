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

#include "caf/scoped_actor.hpp"

#include "caf/policy/no_resume.hpp"
#include "caf/policy/no_scheduling.hpp"
#include "caf/policy/actor_policies.hpp"
#include "caf/policy/nestable_invoke.hpp"
#include "caf/policy/not_prioritizing.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/proper_actor.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {

namespace {

struct impl : blocking_actor {
  void act() override {
    CAF_LOG_ERROR("act() of scoped_actor impl called");
  }
};

blocking_actor* alloc() {
  using namespace policy;
  using policies = actor_policies<no_scheduling, not_prioritizing,
                                  no_resume, nestable_invoke>;
  return new detail::proper_actor<impl, policies>;
}

} // namespace <anonymous>

void scoped_actor::init(bool hide_actor) {
  m_self.reset(alloc());
  if (!hide_actor) {
    m_prev = CAF_SET_AID(m_self->id());
  }
  CAF_LOG_TRACE(CAF_ARG(hide_actor));
  m_self->is_registered(!hide_actor);
}

scoped_actor::scoped_actor() {
  init(false);
}

scoped_actor::scoped_actor(bool hide_actor) {
  init(hide_actor);
}

scoped_actor::~scoped_actor() {
  CAF_LOG_TRACE("");
  if (m_self->is_registered()) {
    CAF_SET_AID(m_prev);
  }
  auto r = m_self->planned_exit_reason();
  m_self->cleanup(r == exit_reason::not_exited ? exit_reason::normal : r);
}

} // namespace caf
