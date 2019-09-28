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

#include <vector>

#include "caf/default_downstream_manager.hpp"
#include "caf/detail/stream_stage_driver_impl.hpp"
#include "caf/detail/stream_stage_impl.hpp"
#include "caf/downstream_manager.hpp"
#include "caf/fwd.hpp"
#include "caf/make_stage_result.hpp"
#include "caf/policy/arg.hpp"
#include "caf/stream.hpp"
#include "caf/stream_stage.hpp"
#include "caf/unit.hpp"

namespace caf {

/// Attaches a new stream stage to `self` by creating a default stream stage
/// manager with `Driver`.
/// @param self Points to the hosting actor.
/// @param in Stream handshake from upstream path.
/// @param xs User-defined arguments for the downstream handshake.
/// @param ys Additional constructor arguments for `Driver`.
/// @returns The new `stream_manager`, an inbound slot, and an outbound slot.
template <class Driver, class In, class... Ts, class... Us>
make_stage_result_t<In, typename Driver::downstream_manager_type, Ts...>
attach_stream_stage(scheduled_actor* self, const stream<In>& in,
                    std::tuple<Ts...> xs, Us&&... ys) {
  using detail::make_stream_stage;
  auto mgr = make_stream_stage<Driver>(self, std::forward<Us>(ys)...);
  auto islot = mgr->add_inbound_path(in);
  auto oslot = mgr->add_outbound_path(std::move(xs));
  return {islot, oslot, std::move(mgr)};
}

/// Attaches a new stream stage to `self` by creating a default stream stage
/// manager from given callbacks.
/// @param self Points to the hosting actor.
/// @param in Stream handshake from upstream path.
/// @param xs User-defined arguments for the downstream handshake.
/// @param init Function object for initializing the state of the stage.
/// @param fun Processing function.
/// @param fin Optional cleanup handler.
/// @param token Policy token for selecting a downstream manager
///              implementation.
/// @returns The new `stream_manager`, an inbound slot, and an outbound slot.
template <class In, class... Ts, class Init, class Fun, class Finalize = unit_t,
          class DownstreamManager = default_downstream_manager_t<Fun>,
          class Trait = stream_stage_trait_t<Fun>>
make_stage_result_t<In, DownstreamManager, Ts...>
attach_stream_stage(scheduled_actor* self, const stream<In>& in,
                    std::tuple<Ts...> xs, Init init, Fun fun, Finalize fin = {},
                    policy::arg<DownstreamManager> token = {}) {
  CAF_IGNORE_UNUSED(token);
  using output_type = typename stream_stage_trait_t<Fun>::output;
  using state_type = typename stream_stage_trait_t<Fun>::state;
  static_assert(std::is_same<
                  void(state_type&),
                  typename detail::get_callable_trait<Init>::fun_sig>::value,
                "Expected signature `void (State&)` for init function");
  using consume_one = void(state_type&, downstream<output_type>&, In);
  using consume_all = void(state_type&, downstream<output_type>&,
                           std::vector<In>&);
  using fun_sig = typename detail::get_callable_trait<Fun>::fun_sig;
  static_assert(std::is_same<fun_sig, consume_one>::value
                  || std::is_same<fun_sig, consume_all>::value,
                "Expected signature `void (State&, downstream<Out>&, In)` "
                "or `void (State&, downstream<Out>&, std::vector<In>&)` "
                "for consume function");
  using driver = detail::stream_stage_driver_impl<
    typename Trait::input, DownstreamManager, Fun, Finalize>;
  return attach_stream_stage<driver>(self, in, std::move(xs), std::move(init),
                                     std::move(fun), std::move(fin));
}

/// Attaches a new stream stage to `self` by creating a default stream stage
/// manager from given callbacks.
/// @param self Points to the hosting actor.
/// @param in Stream handshake from upstream path.
/// @param init Function object for initializing the state of the stage.
/// @param fun Processing function.
/// @param fin Optional cleanup handler.
/// @param token Policy token for selecting a downstream manager
///              implementation.
/// @returns The new `stream_manager`, an inbound slot, and an outbound slot.
template <class In, class Init, class Fun, class Finalize = unit_t,
          class DownstreamManager = default_downstream_manager_t<Fun>,
          class Trait = stream_stage_trait_t<Fun>>
make_stage_result_t<In, DownstreamManager>
attach_stream_stage(scheduled_actor* self, const stream<In>& in, Init init,
                    Fun fun, Finalize fin = {},
                    policy::arg<DownstreamManager> token = {}) {
  return attach_stream_stage(self, in, std::make_tuple(), std::move(init),
                             std::move(fun), std::move(fin), token);
}

} // namespace caf
