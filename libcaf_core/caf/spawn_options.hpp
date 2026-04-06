// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/detail/formatted.hpp"

namespace caf {

/// @addtogroup ActorCreation
/// @{

/// Stores options passed to the `spawn` function family.
enum class spawn_options : int {
  no_flags = 0x00,
  priority_aware_flag CAF_DEPRECATED("no longer has any effect") = 0x00,
  link_flag = 0x01,
  detach_flag = 0x02,
  hide_flag = 0x04,
  lazy_init_flag = 0x08,
  blocking_flag = 0x80,
};

/// Concatenates two @ref spawn_options.
constexpr spawn_options operator+(spawn_options x, spawn_options y) noexcept {
  return static_cast<spawn_options>(static_cast<int>(x) | static_cast<int>(y));
}

/// Denotes default settings.
constexpr spawn_options no_spawn_options = spawn_options::no_flags;

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
constexpr bool has_spawn_option(spawn_options haystack,
                                spawn_options needle) noexcept {
  return (static_cast<int>(haystack) & static_cast<int>(needle)) != 0;
}

/// Checks whether the @ref detached flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref detached flag is set in `opts`, otherwise
///          `false`.
constexpr bool has_detach_flag(spawn_options opts) noexcept {
  return has_spawn_option(opts, detached);
}

CAF_DEPRECATED("the priority_aware flag no longer exists")
constexpr bool has_priority_aware_flag(spawn_options) noexcept {
  return true;
}

/// Checks whether the @ref hidden flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref hidden flag is set in `opts`, otherwise
constexpr bool has_hide_flag(spawn_options opts) noexcept {
  return has_spawn_option(opts, hidden);
}

/// Checks whether the @ref linked flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref linked flag is set in `opts`, otherwise
constexpr bool has_link_flag(spawn_options opts) noexcept {
  return has_spawn_option(opts, linked);
}

/// Checks whether the @ref lazy_init flag is set in `opts`.
/// @param opts Bitmask to search in.
/// @returns `true` if the @ref lazy_init flag is set in `opts`, otherwise
constexpr bool has_lazy_init_flag(spawn_options opts) noexcept {
  return has_spawn_option(opts, lazy_init);
}

/// @}

/// @cond

constexpr bool is_unbound(spawn_options opts) noexcept {
  return !has_link_flag(opts);
}

constexpr spawn_options make_unbound(spawn_options opts) noexcept {
  return static_cast<spawn_options>(
    (static_cast<int>(opts) & ~static_cast<int>(linked)));
}

/// @endcond

} // namespace caf

namespace caf::detail {

template <>
struct simple_formatter<spawn_options> {
  template <class OutputIterator>
  OutputIterator format(spawn_options opts, OutputIterator out) const {
    *out++ = '[';
    auto add = [&out, first = true](const char* name) mutable {
      if (first) {
        first = false;
      } else {
        *out++ = ',';
        *out++ = ' ';
      }
      for (auto c = *name; c != '\0'; c = *++name) {
        *out++ = c;
      }
    };
    if (has_spawn_option(opts, spawn_options::link_flag)) {
      add("link");
    }
    if (has_spawn_option(opts, spawn_options::detach_flag)) {
      add("detach");
    }
    if (has_spawn_option(opts, spawn_options::hide_flag)) {
      add("hide");
    }
    if (has_spawn_option(opts, spawn_options::lazy_init_flag)) {
      add("lazy_init");
    }
    if (has_spawn_option(opts, spawn_options::blocking_flag)) {
      add("blocking");
    }
    *out++ = ']';
    return out;
  }
};

} // namespace caf::detail
