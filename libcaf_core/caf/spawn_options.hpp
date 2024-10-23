// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// @addtogroup ActorCreation
/// @{

/// Stores options passed to the `spawn` function family.
enum class spawn_options : int {
  no_flags = 0x00,
  link_flag = 0x01,
  monitor_flag = 0x02,
  detach_flag = 0x04,
  hide_flag = 0x08,
  priority_aware_flag = 0x20,
  lazy_init_flag = 0x40
};

/// Concatenates two @ref spawn_options.
constexpr spawn_options operator+(spawn_options x, spawn_options y) {
  return static_cast<spawn_options>(static_cast<int>(x) | static_cast<int>(y));
}

/// Denotes default settings.
constexpr spawn_options no_spawn_options = spawn_options::no_flags;

/// Causes `spawn` to call `self->monitor(...)` immediately after the new actor
/// was spawned.
[[deprecated("call monitor directly instead")]]
constexpr spawn_options monitored
  = spawn_options::monitor_flag;

/// Causes `spawn` to call `self->link_to(...)` immediately
/// after the new actor was spawned.
constexpr spawn_options linked = spawn_options::link_flag;

/// Causes the new actor to opt out of the cooperative scheduling.
constexpr spawn_options detached = spawn_options::detach_flag;

/// Causes the runtime to ignore the new actor in `await_all_actors_done()`.
constexpr spawn_options hidden = spawn_options::hide_flag;

/// Causes the new actor to delay its
/// initialization until a message arrives.
constexpr spawn_options lazy_init = spawn_options::lazy_init_flag;

/// Checks whether `haystack` contains `needle`.
/// @param haystack Bitmask to search in.
/// @param needle Flag to search for.
/// @returns `true` if `needle` is set in `haystack`, otherwise `false`.
constexpr bool has_spawn_option(spawn_options haystack, spawn_options needle) {
  return (static_cast<int>(haystack) & static_cast<int>(needle)) != 0;
}

/// Checks whether the @ref detached flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref detached flag is set in `opts`, otherwise
///          `false`.
constexpr bool has_detach_flag(spawn_options opts) {
  return has_spawn_option(opts, detached);
}

/// Obsolete, since the `priority_aware` flag no longer exists.
/// @param opts Bitmask to search in.
/// @returns `true`.
constexpr bool has_priority_aware_flag([[maybe_unused]] spawn_options opts) {
  return true;
}

/// Checks whether the @ref hidden flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref hidden flag is set in `opts`, otherwise
constexpr bool has_hide_flag(spawn_options opts) {
  return has_spawn_option(opts, hidden);
}

/// Checks whether the @ref linked flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref linked flag is set in `opts`, otherwise
constexpr bool has_link_flag(spawn_options opts) {
  return has_spawn_option(opts, linked);
}

/// Checks whether the @ref monitored flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref monitored flag is set in `opts`, otherwise
constexpr bool has_monitor_flag(spawn_options opts) {
  return has_spawn_option(opts, spawn_options::monitor_flag);
}

/// Checks whether the @ref lazy_init flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref lazy_init flag is set in `opts`, otherwise
constexpr bool has_lazy_init_flag(spawn_options opts) {
  return has_spawn_option(opts, lazy_init);
}

/// @}

/// @cond

constexpr bool is_unbound(spawn_options opts) {
  return !has_monitor_flag(opts) && !has_link_flag(opts);
}

constexpr spawn_options make_unbound(spawn_options opts) {
  return static_cast<spawn_options>(
    (static_cast<int>(opts)
     & ~(static_cast<int>(linked)
         | static_cast<int>(spawn_options::monitor_flag))));
}

/// @endcond

} // namespace caf
