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

#pragma once

namespace caf {

/// @addtogroup ActorCreation
/// @{

/// Stores options passed to the `spawn` function family.
#ifdef CAF_DOCUMENTATION
class spawn_options {};
#else
enum class spawn_options : int {
  no_flags = 0x00,
  link_flag = 0x01,
  monitor_flag = 0x02,
  detach_flag = 0x04,
  hide_flag = 0x08,
  priority_aware_flag = 0x20,
  lazy_init_flag = 0x40
};
#endif

/// Concatenates two {@link spawn_options}.
/// @relates spawn_options
constexpr spawn_options operator+(const spawn_options& lhs,
                  const spawn_options& rhs) {
  return static_cast<spawn_options>(static_cast<int>(lhs) |
                    static_cast<int>(rhs));
}

/// Denotes default settings.
constexpr spawn_options no_spawn_options = spawn_options::no_flags;

/// Causes `spawn` to call `self->monitor(...) immediately
/// after the new actor was spawned.
constexpr spawn_options monitored = spawn_options::monitor_flag;

/// Causes `spawn` to call `self->link_to(...) immediately
/// after the new actor was spawned.
constexpr spawn_options linked = spawn_options::link_flag;

/// Causes the new actor to opt out of the cooperative scheduling.
constexpr spawn_options detached = spawn_options::detach_flag;

/// Causes the runtime to ignore the new actor in `await_all_actors_done()`.
constexpr spawn_options hidden = spawn_options::hide_flag;

/// Causes the new actor to evaluate message priorities.
constexpr spawn_options priority_aware CAF_DEPRECATED =
  spawn_options::priority_aware_flag;

/// Causes the new actor to delay its
/// initialization until a message arrives.
constexpr spawn_options lazy_init = spawn_options::lazy_init_flag;

/// Checks wheter `haystack` contains `needle`.
/// @relates spawn_options
constexpr bool has_spawn_option(spawn_options haystack, spawn_options needle) {
  return (static_cast<int>(haystack) & static_cast<int>(needle)) != 0;
}

/// Checks wheter the {@link detached} flag is set in `opts`.
/// @relates spawn_options
constexpr bool has_detach_flag(spawn_options opts) {
  return has_spawn_option(opts, detached);
}

/// Checks wheter the {@link priority_aware} flag is set in `opts`.
/// @relates spawn_options
constexpr bool has_priority_aware_flag(spawn_options) {
  return true;
}

/// Checks wheter the {@link hidden} flag is set in `opts`.
/// @relates spawn_options
constexpr bool has_hide_flag(spawn_options opts) {
  return has_spawn_option(opts, hidden);
}

/// Checks wheter the {@link linked} flag is set in `opts`.
/// @relates spawn_options
constexpr bool has_link_flag(spawn_options opts) {
  return has_spawn_option(opts, linked);
}

/// Checks wheter the {@link monitored} flag is set in `opts`.
/// @relates spawn_options
constexpr bool has_monitor_flag(spawn_options opts) {
  return has_spawn_option(opts, monitored);
}

/// Checks wheter the {@link lazy_init} flag is set in `opts`.
/// @relates spawn_options
constexpr bool has_lazy_init_flag(spawn_options opts) {
  return has_spawn_option(opts, lazy_init);
}

/// @}

/// @cond PRIVATE

constexpr bool is_unbound(spawn_options opts) {
  return !has_monitor_flag(opts) && !has_link_flag(opts);
}

constexpr spawn_options make_unbound(spawn_options opts) {
  return static_cast<spawn_options>(
    (static_cast<int>(opts) &
     ~(static_cast<int>(linked) | static_cast<int>(monitored))));
}

/// @endcond

} // namespace caf

