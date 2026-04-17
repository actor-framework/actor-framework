// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/scoped_actor.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_registry.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/log/system.hpp"
#include "caf/spawn_options.hpp"

namespace caf {

namespace {

class impl : public blocking_actor {
public:
  using super = blocking_actor;

  static constexpr auto forced_spawn_options = spawn_options::blocking_flag;

  explicit impl(actor_config& cfg)
    : super(cfg.add_flag(abstract_actor::is_scoped_actor_impl_flag)) {
    // nop
  }

  void act() override {
    detail::critical("act() of scoped_actor impl called");
  }

  const char* name() const override {
    return "scoped_actor";
  }

  void launch([[maybe_unused]] detail::private_thread* worker,
              scheduler* ctx) override {
    CAF_ASSERT(worker == nullptr);
    detail::current_actor_guard guard{this};
    auto lg = log::system::trace("");
    CAF_ASSERT(getf(is_blocking_flag));
    initialize(ctx);
  }

  void do_monitor(abstract_actor* ptr, message_priority prio) override {
    detail::current_actor_guard guard{this};
    super::do_monitor(ptr, prio);
  }

  void do_demonitor(const strong_actor_ptr& whom) override {
    detail::current_actor_guard guard{this};
    super::do_demonitor(whom);
  }

  void add_link(abstract_actor* other) override {
    detail::current_actor_guard guard{this};
    super::add_link(other);
  }

  void remove_link(abstract_actor* other) override {
    detail::current_actor_guard guard{this};
    super::remove_link(other);
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
}

scoped_actor& scoped_actor::operator=(scoped_actor&& other) noexcept {
  if (this != &other) {
    cleanup();
    self_ = std::move(other.self_);
  }
  return *this;
}

scoped_actor::~scoped_actor() noexcept {
  cleanup();
}

blocking_actor* scoped_actor::ptr() const {
  return actor_cast<blocking_actor*>(self_);
}

void scoped_actor::cleanup() noexcept {
  if (!self_)
    return;
  auto* self = ptr();
  if (!self->getf(abstract_actor::is_terminated_flag)) {
    detail::current_actor_guard guard{self};
    self->cleanup(exit_reason::normal, nullptr);
  }
}

std::string to_string(const scoped_actor& x) {
  return to_string(x.address());
}

} // namespace caf
