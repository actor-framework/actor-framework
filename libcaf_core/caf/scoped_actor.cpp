// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scoped_actor.hpp"

#include "caf/actor_registry.hpp"
#include "caf/log/system.hpp"
#include "caf/spawn_options.hpp"

namespace caf {

namespace {

class impl : public blocking_actor {
public:
  impl(actor_config& cfg) : blocking_actor(cfg) {
    // nop
  }

  void act() override {
    log::system::error("act() of scoped_actor impl called");
  }

  const char* name() const override {
    return "scoped_actor";
  }

  void launch(scheduler*, bool, bool hide) override {
    CAF_PUSH_AID_FROM_PTR(this);
    std::ignore = log::system::trace("hide = {}", hide);
    CAF_ASSERT(getf(is_blocking_flag));
    if (!hide)
      register_at_system();
    initialize();
  }
};

} // namespace

scoped_actor::scoped_actor(actor_system& sys, bool hide) {
  CAF_SET_LOGGER_SYS(&sys);
  actor_config cfg;
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
    x->cleanup(exit_reason::normal, nullptr);
  CAF_SET_AID(prev_);
}

blocking_actor* scoped_actor::ptr() const {
  return static_cast<blocking_actor*>(actor_cast<abstract_actor*>(self_));
}

std::string to_string(const scoped_actor& x) {
  return to_string(x.address());
}

} // namespace caf
