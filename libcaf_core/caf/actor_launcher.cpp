// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_launcher.hpp"

#include "caf/actor_cast.hpp"
#include "caf/local_actor.hpp"

namespace caf {

actor_launcher::actor_launcher(strong_actor_ptr self, caf::scheduler* context,
                               spawn_options options)
  : ready_(true) {
  new (&state_) state{std::move(self), context, options};
}

actor_launcher::actor_launcher(actor_launcher&& other) noexcept
  : ready_(other.ready_) {
  if (ready_) {
    new (&state_) state(std::move(other.state_));
    other.reset();
  }
}

actor_launcher& actor_launcher::operator=(actor_launcher&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (ready_) {
    reset();
  }
  if (other.ready_) {
    ready_ = true;
    new (&state_) state(std::move(other.state_));
    other.reset();
  }
  return *this;
}

actor_launcher::~actor_launcher() {
  (*this)();
}

void actor_launcher::operator()() {
  if (!ready_) {
    return;
  }
  if (auto* ptr = actor_cast<local_actor*>(state_.self)) {
    ptr->unsetf(abstract_actor::is_inactive_flag);
    ptr->launch(state_.context, has_lazy_init_flag(state_.options),
                has_hide_flag(state_.options));
  }
  reset();
}

void actor_launcher::reset() {
  CAF_ASSERT(ready_);
  ready_ = false;
  state_.~state();
}

} // namespace caf
