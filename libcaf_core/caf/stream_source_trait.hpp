// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
struct stream_source_trait<void(State&, downstream<T>&, size_t)> {
  static constexpr bool valid = true;
  using output = T;
  using state = State;
};

/// Convenience alias for extracting the function signature from `Pull` and
/// passing it to `stream_source_trait`.
template <class Pull>
using stream_source_trait_t
  = stream_source_trait<typename detail::get_callable_trait<Pull>::fun_sig>;

} // namespace caf
