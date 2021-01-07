// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>
#include <utility>

#include "caf/meta/annotation.hpp"

namespace caf::meta {

template <class F>
struct load_callback_t : annotation {
  load_callback_t(F&& f) : fun(f) {
    // nop
  }

  load_callback_t(load_callback_t&&) = default;

  load_callback_t(const load_callback_t&) = default;

  F fun;
};

template <class T>
struct is_load_callback : std::false_type {};

template <class F>
struct is_load_callback<load_callback_t<F>> : std::true_type {};

template <class F>
constexpr bool is_load_callback_v = is_load_callback<F>::value;

/// Returns an annotation that allows inspectors to call
/// user-defined code after performing load operations.
template <class F>
load_callback_t<F> load_callback(F fun) {
  return {std::move(fun)};
}

} // namespace caf::meta
