// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_module.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/settings.hpp"
#include "caf/spawn_options.hpp"
#include <caf/send.hpp>

#include <functional>
#include <tuple>

namespace caf::detail {

/// A module that starts and manages background threads.
class CAF_CORE_EXPORT daemons : public actor_system_module {
public:
  daemons(actor_system& sys);

  daemons(const daemons&) = delete;

  daemons& operator=(const daemons&) = delete;

  ~daemons() override;

  /// Launches a new hidden (and detached) background worker.
  /// @param fn Function object that implements the worker actor.
  /// @param do_stop Function object that stops the worker actor. The function
  ///                object is called with the actor handle as argument to allow
  ///                sending an exit message.
  /// @param args Additional arguments forwarded to `fn`.
  template <class Fn, class... Args>
  actor launch(Fn fn, std::function<void(actor)> do_stop, Args&&... args) {
    // Launches a new background worker. We need to this lazily to enable
    // `do_launch` to atomically create the worker and add it to the internal
    // map. If `stop()` has been called prior, we return an invalid actor handle
    // and never call `do_spawn`.
    auto do_spawn = [args = std::make_tuple(std::forward<Args>(args)...),
                     fn = std::move(fn)](actor_system& sys) {
      // TODO: simply capture args... in the lambda when we switch to C++20
      return std::apply(
        [&fn, &sys](auto&&... xs) {
          return sys.spawn<hidden + detached>(std::move(fn), xs...);
        },
        std::move(args));
    };
    return do_launch(do_spawn, do_stop);
  }

  // -- overrides --------------------------------------------------------------

  void start() override;

  void stop() override;

  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

private:
  actor do_launch(std::function<actor(actor_system&)> do_spawn,
                  std::function<void(actor)> do_stop);

  struct impl;
  impl* impl_;
};

} // namespace caf::detail
