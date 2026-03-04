// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/scoped_actor.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_registry.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/log/system.hpp"
#include "caf/spawn_options.hpp"

namespace caf {

namespace {

class impl : public blocking_actor {
public:
  static constexpr auto forced_spawn_options = spawn_options::blocking_flag;

  explicit impl(actor_config& cfg)
    : blocking_actor(cfg.add_flag(abstract_actor::is_scoped_actor_impl_flag)) {
    // nop
  }

  void act() override {
    log::system::error("act() of scoped_actor impl called");
  }

  const char* name() const override {
    return "scoped_actor";
  }

  void launch([[maybe_unused]] detail::private_thread* worker,
              scheduler* ctx) override {
    CAF_ASSERT(worker == nullptr);
    detail::current_actor_guard ctx_guard{this};
    auto lg = log::system::trace("");
    CAF_ASSERT(getf(is_blocking_flag));
    initialize(ctx);
  }
};

} // namespace

scoped_actor::scoped_actor(actor_system& sys, bool hide) {
  CAF_SET_LOGGER_SYS(&sys);
  auto do_spawn = [&sys, hide] {
    if (hide) {
      return sys.spawn<impl, hidden>();
    }
    return sys.spawn<impl>();
  };
  self_ = actor_cast<strong_actor_ptr>(do_spawn());
  prev_ = detail::current_actor();
  detail::current_actor(self_->get());
}

scoped_actor::~scoped_actor() {
  if (!self_)
    return;
  auto x = ptr();
  if (!x->getf(abstract_actor::is_terminated_flag))
    x->cleanup(exit_reason::normal, nullptr);
  detail::current_actor(prev_);
}

blocking_actor* scoped_actor::ptr() const {
  return actor_cast<blocking_actor*>(self_);
}

std::string to_string(const scoped_actor& x) {
  return to_string(x.address());
}

} // namespace caf
