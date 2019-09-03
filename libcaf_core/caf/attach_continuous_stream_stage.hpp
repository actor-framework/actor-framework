/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include "caf/default_downstream_manager.hpp"
#include "caf/detail/stream_stage_driver_impl.hpp"
#include "caf/detail/stream_stage_impl.hpp"
#include "caf/downstream_manager.hpp"
#include "caf/fwd.hpp"
#include "caf/make_stage_result.hpp"
#include "caf/policy/arg.hpp"
#include "caf/stream.hpp"
#include "caf/stream_source.hpp"
#include "caf/unit.hpp"

namespace caf {

/// Returns a stream manager (implementing a continuous stage) without in- or
/// outbound path. The returned manager is not connected to any slot and thus
/// not stored by the actor automatically.
/// @param self Points to the hosting actor.
/// @param xs User-defined arguments for the downstream handshake.
/// @returns The new `stream_manager`.
template <class Driver, class... Ts>
typename Driver::stage_ptr_type
attach_continuous_stream_stage(scheduled_actor* self, Ts&&... xs) {
  auto ptr = detail::make_stream_stage<Driver>(self, std::forward<Ts>(xs)...);
  ptr->continuous(true);
  return ptr;
}

/// @param self Points to the hosting actor.
/// @param init Function object for initializing the state of the stage.
/// @param fun Processing function.
/// @param fin Optional cleanup handler.
/// @param token Policy token for selecting a downstream manager
///              implementation.
template <class Init, class Fun, class Finalize = unit_t,
          class DownstreamManager = default_downstream_manager_t<Fun>,
          class Trait = stream_stage_trait_t<Fun>>
stream_stage_ptr<typename Trait::input, DownstreamManager>
attach_continuous_stream_stage(scheduled_actor* self, Init init, Fun fun,
                               Finalize fin = {},
                               policy::arg<DownstreamManager> token = {}) {
  CAF_IGNORE_UNUSED(token);
  using input_type = typename Trait::input;
  using output_type = typename Trait::output;
  using state_type = typename Trait::state;
  static_assert(std::is_same<
                  void(state_type&),
                  typename detail::get_callable_trait<Init>::fun_sig>::value,
                "Expected signature `void (State&)` for init function");
  static_assert(std::is_same<
                  void(state_type&, downstream<output_type>&, input_type),
                  typename detail::get_callable_trait<Fun>::fun_sig>::value,
                "Expected signature `void (State&, downstream<Out>&, In)` "
                "for consume function");
  using detail::stream_stage_driver_impl;
  using driver = stream_stage_driver_impl<typename Trait::input,
                                          DownstreamManager, Fun, Finalize>;
  return attach_continuous_stream_stage<driver>(self, std::move(init),
                                                std::move(fun), std::move(fin));
}

} // namespace caf
