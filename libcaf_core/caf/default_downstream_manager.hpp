// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/broadcast_downstream_manager.hpp"
#include "caf/stream_source_trait.hpp"
#include "caf/stream_stage_trait.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Selects a downstream manager implementation based on the signature of
/// various handlers.
template <class F>
struct default_downstream_manager {
  /// The function signature of `F`.
  using fun_sig = typename detail::get_callable_trait<F>::fun_sig;

  /// The source trait for `F`.
  using source_trait = stream_source_trait<fun_sig>;

  /// The stage trait for `F`.
  using stage_trait = stream_stage_trait<fun_sig>;

  /// The output type as returned by the source or stage trait.
  using output_type =
    typename std::conditional<
      source_trait::valid,
      typename source_trait::output,
      typename stage_trait::output
    >::type;

  /// The default downstream manager deduced by this trait.
  using type = broadcast_downstream_manager<output_type>;
};

template <class F>
using default_downstream_manager_t =
  typename default_downstream_manager<F>::type;

} // namespace caf

