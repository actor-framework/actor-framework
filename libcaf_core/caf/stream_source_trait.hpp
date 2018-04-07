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

#include "caf/fwd.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Deduces the output type and the state type for a stream source from its
/// `pull` implementation.
template <class Pull>
struct stream_source_trait {
  static constexpr bool valid = false;
  using output = unit_t;
  using state = unit_t;
};

template <class State, class T>
struct stream_source_trait<void (State&, downstream<T>&, size_t)> {
  static constexpr bool valid = true;
  using output = T;
  using state = State;
};

/// Convenience alias for extracting the function signature from `Pull` and
/// passing it to `stream_source_trait`.
template <class Pull>
using stream_source_trait_t =
  stream_source_trait<typename detail::get_callable_trait<Pull>::fun_sig>;

} // namespace caf

