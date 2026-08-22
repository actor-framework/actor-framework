// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <caf/actor_system.hpp>
#include <caf/detail/actor_system_access.hpp>
#include <caf/detail/scope_guard.hpp>

#include <thread>

namespace caf::detail {

template <class F>
std::thread launch_thread(actor_system_impl* sys, const char* thread_name,
                          thread_owner tag, F fun) {
  auto body = [sys, thread_name, tag, f = std::move(fun)](auto) {
    logger::current_logger(sys);
    detail::set_thread_name(thread_name);
    sys->thread_started(tag);
    auto guard = detail::scope_guard{[sys]() noexcept { //
      sys->thread_terminates();
    }};
    f();
  };
  return std::thread{std::move(body), sys->meta_objects_guard()};
}

} // namespace caf::detail

namespace caf {

/// Launches a new thread on the actor system, making sure the new thread
/// invokes `thread_started` and `thread_terminates` callbacks on the actor
/// system.
template <class F>
std::thread launch_thread(actor_system& sys, const char* thread_name,
                          thread_owner tag, F fun) {
  detail::actor_system_access access{sys};
  return detail::launch_thread(access.impl(), thread_name, tag, std::move(fun));
}

} // namespace caf
