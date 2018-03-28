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

#include "caf/meta/annotation.hpp"

namespace caf {
namespace meta {

template <class F>
struct save_callback_t : annotation {
  save_callback_t(F&& f) : fun(f) {
    // nop
  }

  save_callback_t(save_callback_t&&) = default;

  save_callback_t(const save_callback_t&) = default;

  F fun;
};

/// Returns an annotation that allows inspectors to call
/// user-defined code after performing save operations.
template <class F>
save_callback_t<F> save_callback(F fun) {
  return {std::move(fun)};
}

} // namespace meta
} // namespace caf

