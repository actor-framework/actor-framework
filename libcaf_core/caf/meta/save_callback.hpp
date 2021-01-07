// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/meta/annotation.hpp"

namespace caf::meta {

template <class F>
struct save_callback_t : annotation {
  save_callback_t(F&& f) : fun(f) {
    // nop
  }

  save_callback_t(save_callback_t&&) = default;

  save_callback_t(const save_callback_t&) = default;

  F fun;
};

template <class T>
struct is_save_callback : std::false_type {};

template <class F>
struct is_save_callback<save_callback_t<F>> : std::true_type {};

template <class F>
constexpr bool is_save_callback_v = is_save_callback<F>::value;

/// Returns an annotation that allows inspectors to call
/// user-defined code after performing save operations.
template <class F>
save_callback_t<F> save_callback(F fun) {
  return {std::move(fun)};
}

} // namespace caf::meta
