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

#include "caf/scoped_actor.hpp"

#include "caf/to_string.hpp"
#include "caf/spawn_options.hpp"
#include "caf/actor_registry.hpp"
#include "caf/scoped_execution_unit.hpp"

namespace caf {

namespace {

class impl : public blocking_actor {
public:
  impl(actor_config& cfg) : blocking_actor(cfg) {
    // nop
  }

  void act() override {
    CAF_LOG_ERROR("act() of scoped_actor impl called");
  }

  const char* name() const override {
    return "scoped_actor";
  }

  void launch(execution_unit*, bool, bool hide) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE(CAF_ARG(hide));
    CAF_ASSERT(getf(is_blocking_flag));
    if (!hide)
      register_at_system();
    initialize();
  }
};

} // namespace <anonymous>

scoped_actor::scoped_actor(actor_system& sys, bool hide) : context_(&sys) {
  CAF_SET_LOGGER_SYS(&sys);
  actor_config cfg{&context_};
  if (hide)
    cfg.flags |= abstract_actor::is_hidden_flag;
  auto hdl = sys.spawn_impl<impl, no_spawn_options>(cfg);
  self_ = actor_cast<strong_actor_ptr>(std::move(hdl));
  prev_ = CAF_SET_AID(self_->id());
}

scoped_actor::~scoped_actor() {
  if (!self_)
    return;
  auto x = ptr();
  if (!x->getf(abstract_actor::is_terminated_flag))
    x->cleanup(exit_reason::normal, &context_);
  CAF_SET_AID(prev_);
}

blocking_actor* scoped_actor::ptr() const {
  return static_cast<blocking_actor*>(actor_cast<abstract_actor*>(self_));
}

std::string to_string(const scoped_actor& x) {
  return to_string(x.address());
}

} // namespace caf
